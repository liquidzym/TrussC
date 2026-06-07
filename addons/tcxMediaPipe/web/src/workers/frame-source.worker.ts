type FrameSourceInboundMessage = {
  type: "start";
  track: MediaStreamTrack;
};

type FrameSourceStatusMessage = {
  type: "status";
  stage: string;
  title: string;
  detail: string;
};

type FrameSourceFrameMessage = {
  type: "frame";
  timestampMs: number;
  frame: ImageBitmap;
};

type TrackFrame = ImageBitmapSource & { close?: () => void };
type TrackFrameReader = ReadableStreamDefaultReader<TrackFrame>;
type TrackProcessorLike = { readable: ReadableStream<TrackFrame> };
type TrackProcessorCtor = new (init: { track: MediaStreamTrack }) => TrackProcessorLike;

const workerGlobal = self as DedicatedWorkerGlobalScope & {
  MediaStreamTrackProcessor?: TrackProcessorCtor;
};

let reader: TrackFrameReader | null = null;
let running = false;

function postStatus(stage: string, title: string, detail: string): void {
  const message: FrameSourceStatusMessage = { type: "status", stage, title, detail };
  self.postMessage(message);
}

async function pumpFrames(): Promise<void> {
  if (!reader || running) {
    return;
  }

  running = true;
  try {
    while (running && reader) {
      const result = await reader.read();
      if (result.done || !result.value) {
        postStatus("camera_worker_reader_ended", "Camera frame reader ended", "The MediaStreamTrackProcessor stream ended.");
        break;
      }

      const source = result.value;
      try {
        const frame = await createImageBitmap(source as ImageBitmapSource);
        const message: FrameSourceFrameMessage = {
          type: "frame",
          timestampMs: performance.now(),
          frame
        };
        self.postMessage(message, [frame]);
      } finally {
        source.close?.();
      }
    }
  } catch (error) {
    const reason = error instanceof Error ? error.message : "MediaStreamTrackProcessor failed while reading frames.";
    postStatus("camera_worker_reader_error", "Camera frame reader error", reason);
  } finally {
    running = false;
    reader = null;
  }
}

self.onmessage = (event: MessageEvent<FrameSourceInboundMessage>) => {
  const message = event.data;
  if (message.type !== "start") {
    return;
  }

  const TrackProcessor = workerGlobal.MediaStreamTrackProcessor;
  if (!TrackProcessor) {
    postStatus(
      "camera_worker_no_track_processor",
      "Camera worker frame source unavailable",
      "MediaStreamTrackProcessor is not available in Dedicated Worker."
    );
    return;
  }

  try {
    const processor = new TrackProcessor({ track: message.track });
    reader = processor.readable.getReader();
    postStatus(
      "camera_worker_track_processor",
      "Camera worker frame source",
      "Reading frames with MediaStreamTrackProcessor in Dedicated Worker."
    );
    void pumpFrames();
  } catch (error) {
    const reason = error instanceof Error ? error.message : "MediaStreamTrackProcessor setup failed.";
    postStatus("camera_worker_setup_error", "Camera worker setup failed", reason);
  }
};
