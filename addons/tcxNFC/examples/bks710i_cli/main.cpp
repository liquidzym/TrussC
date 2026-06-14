#include <tcxNFC.h>

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

struct Args {
    std::string command = "ping";
    std::string host = "192.168.1.100";
    std::string sourceHost;
    std::string url = "https://wstree.cn/t/TEST";
    int timeoutMs = 1000;
};

Args parseArgs(int argc, char** argv) {
    Args args;
    if (argc > 1) {
        args.command = argv[1];
    }
    for (int i = 2; i < argc; ++i) {
        const std::string key = argv[i];
        if (key == "--host" && i + 1 < argc) {
            args.host = argv[++i];
        } else if (key == "--source-host" && i + 1 < argc) {
            args.sourceHost = argv[++i];
        } else if (key == "--url" && i + 1 < argc) {
            args.url = argv[++i];
        } else if (key == "--timeout-ms" && i + 1 < argc) {
            args.timeoutMs = std::atoi(argv[++i]);
        }
    }
    return args;
}

} // namespace

int main(int argc, char** argv) {
    const Args args = parseArgs(argc, argv);

    tcx::nfc::TcpEndpoint endpoint;
    endpoint.host = args.host;
    endpoint.sourceHost = args.sourceHost;
    endpoint.timeoutMs = args.timeoutMs;

    tcx::nfc::TcpSocketTransport privateTcp(endpoint);
    tcx::nfc::TcpSocketTransport modbusTcp(endpoint);
    tcx::nfc::Bks710iReader reader(privateTcp, modbusTcp);

    if (args.command == "ping") {
        auto ping = reader.ping();
        if (!ping.ok) {
            std::cerr << ping.error << '\n';
            return 1;
        }
        std::cout << "reader online\n";
        return 0;
    }

    if (args.command == "read-uid") {
        auto uid = reader.readUid();
        if (!uid.ok) {
            std::cerr << uid.error << '\n';
            return 1;
        }
        std::cout << uid.value.uidHex << '\n';
        return 0;
    }

    if (args.command == "write-url") {
        auto result = reader.writeUrlRawNtag(args.url, 144, 4);
        if (!result.ok) {
            std::cerr << result.error << '\n';
            return 1;
        }
        std::cout << "write " << result.value.writeStrategy
                  << " verification=" << result.value.verificationLevel
                  << " pages=" << result.value.pagesWritten << '\n';
        return result.value.verificationLevel == "verified" ? 0 : 2;
    }

    std::cerr << "usage: tcxNFC_bks710i_cli [ping|read-uid|write-url] [--host ip] [--source-host ip] [--url https://...]\n";
    return 1;
}
