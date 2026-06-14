#pragma once

#include "common/Config.h"

#include <string_view>

namespace maya_rfid {

int configCheck(const AppConfig& config);
int storageCheck(const AppConfig& config);
int tokenStatus(const AppConfig& config);
int syncPeek(const AppConfig& config);
int syncAck(const AppConfig& config);
int syncFail(const AppConfig& config, std::string_view errorMessage);
int tokenAdd(const AppConfig& config, std::string_view token, std::string_view url);
int buildNdef(std::string_view url, int maxUserBytes);
int readerPing(const AppConfig& config);
int readUid(const AppConfig& config);
int writeUrl(const AppConfig& config, std::string_view url);
int readNdef(const AppConfig& config, int startPage, int endPage);

} // namespace maya_rfid
