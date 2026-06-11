#include "tcxsd/Generator.h"

#include <deque>
#include <utility>

namespace tcx::sd {
namespace {

struct Job {
    JobId id = 0;
    ImageRequest request;
};

struct WorkerMessage {
    bool hasProgress = false;
    bool hasResult = false;
    Progress progress;
    ImageResult result;
};

class Worker : public trussc::Thread {
public:
    bool setup(const ModelPaths& paths, const RuntimeSettings& settings, std::string* error) {
        return runtime_.setup(paths, settings, error);
    }

    void enqueue(Job&& job) {
        jobs_.send(std::move(job));
    }

    void requestCancel() {
        cancelRequested_.store(true);
    }

    bool isBusy() const {
        return busy_.load();
    }

    bool receive(WorkerMessage& message) {
        return messages_.tryReceive(message);
    }

    void close() {
        jobs_.close();
        requestCancel();
        stopThread();
        waitForThread(false);
        runtime_.shutdown();
    }

protected:
    void threadedFunction() override {
        while (isThreadRunning()) {
            Job job;
            if (!jobs_.tryReceive(job, 50)) {
                continue;
            }

            busy_.store(true);
            cancelRequested_.store(false);

            WorkerMessage started;
            started.hasProgress = true;
            started.progress.jobId = job.id;
            started.progress.state = JobState::Running;
            started.progress.message = "Generation started";
            messages_.send(std::move(started));

            auto progress = [this, jobId = job.id](const Progress& p) {
                WorkerMessage message;
                message.hasProgress = true;
                message.progress = p;
                message.progress.jobId = jobId;
                messages_.send(std::move(message));
            };

            ImageResult result = runtime_.generateImage(job.id, job.request, progress, cancelRequested_);
            WorkerMessage completed;
            completed.hasResult = true;
            completed.result = std::move(result);
            messages_.send(std::move(completed));

            busy_.store(false);
        }
    }

private:
    NativeRuntime runtime_;
    trussc::ThreadChannel<Job> jobs_;
    trussc::ThreadChannel<WorkerMessage> messages_;
    std::atomic<bool> busy_{false};
    std::atomic<bool> cancelRequested_{false};
};

} // namespace

struct StableDiffusion::Impl {
    std::unique_ptr<Worker> worker;
    std::atomic<JobId> nextJobId{1};
    mutable std::mutex mutex;
    bool ready = false;
    bool running = false;
    std::string lastError;
    Progress currentProgress;
    std::deque<ImageResult> results;
    ProgressCallback progressCallback;
    ResultCallback resultCallback;
};

ImageJobBuilder::ImageJobBuilder(StableDiffusion& generator, ImageRequest request)
    : generator_(&generator)
    , request_(std::move(request)) {
}

ImageJobBuilder& ImageJobBuilder::size(int width, int height) {
    request_.size(width, height);
    return *this;
}

ImageJobBuilder& ImageJobBuilder::square(int side) {
    request_.square(side);
    return *this;
}

ImageJobBuilder& ImageJobBuilder::steps(int value) {
    request_.stepsCount(value);
    return *this;
}

ImageJobBuilder& ImageJobBuilder::seed(std::int64_t value) {
    request_.seedValue(value);
    return *this;
}

ImageJobBuilder& ImageJobBuilder::cfg(float value) {
    request_.cfg(value);
    return *this;
}

ImageJobBuilder& ImageJobBuilder::negative(std::string text) {
    request_.negative(std::move(text));
    return *this;
}

ImageJobBuilder& ImageJobBuilder::draft() {
    request_.draft();
    return *this;
}

ImageJobBuilder& ImageJobBuilder::balanced() {
    request_.balanced();
    return *this;
}

ImageJobBuilder& ImageJobBuilder::final() {
    request_.final();
    return *this;
}

ImageJobBuilder& ImageJobBuilder::metadata(std::string key, std::string value) {
    request_.metadata[std::move(key)] = std::move(value);
    return *this;
}

JobId ImageJobBuilder::run() {
    return generator_ ? generator_->submit(request_) : 0;
}

const ImageRequest& ImageJobBuilder::request() const {
    return request_;
}

StableDiffusion::StableDiffusion()
    : impl_(std::make_unique<Impl>()) {
}

StableDiffusion::~StableDiffusion() {
    shutdown();
}

bool StableDiffusion::nativeAvailable() {
    return NativeRuntime::available();
}

std::string StableDiffusion::nativeSystemInfo() {
    return NativeRuntime::systemInfo();
}

bool StableDiffusion::setup(const ModelPaths& paths, const RuntimeSettings& settings) {
    shutdown();

    auto worker = std::make_unique<Worker>();
    std::string error;
    if (!worker->setup(paths, settings, &error)) {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->ready = false;
        impl_->lastError = error;
        return false;
    }

    worker->startThread();

    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->worker = std::move(worker);
    impl_->ready = true;
    impl_->running = false;
    impl_->lastError.clear();
    impl_->currentProgress = {};
    impl_->results.clear();
    return true;
}

bool StableDiffusion::setupIdeogram4(const fs::path& modelDir, const RuntimeSettings& settings) {
    return setup(ModelPaths::ideogram4Example(modelDir), settings);
}

void StableDiffusion::shutdown() {
    std::unique_ptr<Worker> worker;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        worker = std::move(impl_->worker);
        impl_->ready = false;
        impl_->running = false;
    }
    if (worker) {
        worker->close();
    }
}

bool StableDiffusion::isReady() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->ready;
}

bool StableDiffusion::isRunning() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->running || (impl_->worker && impl_->worker->isBusy());
}

std::string StableDiffusion::lastError() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->lastError;
}

Progress StableDiffusion::progress() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->currentProgress;
}

ImageJobBuilder StableDiffusion::createImage(std::string prompt) {
    return ImageJobBuilder(*this, ImageRequest::fromPrompt(std::move(prompt)));
}

ImageJobBuilder StableDiffusion::createImage(const IdeogramPrompt& promptSpec) {
    return ImageJobBuilder(*this, ImageRequest::fromIdeogram4(promptSpec));
}

JobId StableDiffusion::submit(ImageRequest request) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->ready || !impl_->worker) {
        impl_->lastError = "StableDiffusion is not ready. Call setup() first.";
        return 0;
    }

    Job job;
    job.id = impl_->nextJobId.fetch_add(1);
    job.request = std::move(request);

    impl_->running = true;
    impl_->currentProgress = {};
    impl_->currentProgress.jobId = job.id;
    impl_->currentProgress.state = JobState::Queued;
    impl_->currentProgress.message = "Queued";
    impl_->worker->enqueue(std::move(job));
    return impl_->currentProgress.jobId;
}

void StableDiffusion::cancel() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->worker) {
        impl_->worker->requestCancel();
    }
}

void StableDiffusion::update() {
    Worker* worker = nullptr;
    ProgressCallback progressCallback;
    ResultCallback resultCallback;

    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        worker = impl_->worker.get();
        progressCallback = impl_->progressCallback;
        resultCallback = impl_->resultCallback;
    }

    if (!worker) {
        return;
    }

    WorkerMessage message;
    while (worker->receive(message)) {
        if (message.hasProgress) {
            {
                std::lock_guard<std::mutex> lock(impl_->mutex);
                impl_->currentProgress = message.progress;
                impl_->running = message.progress.state == JobState::Running;
            }
            if (progressCallback) {
                progressCallback(message.progress);
            }
        }

        if (message.hasResult) {
            bool ok = message.result.ok;
            std::string error = message.result.error;
            if (resultCallback) {
                resultCallback(message.result);
            }
            {
                std::lock_guard<std::mutex> lock(impl_->mutex);
                impl_->running = false;
                impl_->currentProgress.jobId = message.result.jobId;
                impl_->currentProgress.state = message.result.state;
                impl_->currentProgress.message = ok ? "Complete" : error;
                if (!ok) {
                    impl_->lastError = error;
                }
                impl_->results.push_back(std::move(message.result));
            }
        }
    }
}

bool StableDiffusion::pollResult(ImageResult& result) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->results.empty()) {
        return false;
    }
    result = std::move(impl_->results.front());
    impl_->results.pop_front();
    return true;
}

bool StableDiffusion::hasResult() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return !impl_->results.empty();
}

void StableDiffusion::onProgress(ProgressCallback callback) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->progressCallback = std::move(callback);
}

void StableDiffusion::onResult(ResultCallback callback) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->resultCallback = std::move(callback);
}

} // namespace tcx::sd
