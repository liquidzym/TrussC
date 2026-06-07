const CAMERA_REQUEST_TIMEOUT_MS = 20000;
const CAMERA_PLAY_TIMEOUT_MS = 5000;
const CAMERA_FIRST_FRAME_TIMEOUT_MS = 10000;

export type CameraStage = {
  stage: string;
  title: string;
  detail: string;
};

export type CameraStartResult = {
  firstFrameReady: boolean;
  stream: MediaStream;
};

type CameraStageReporter = (stage: CameraStage) => void;

function report(onStage: CameraStageReporter | undefined, stage: string, title: string, detail: string): void {
  onStage?.({ stage, title, detail });
}

function cameraTimeout(): { promise: Promise<MediaStream>; cancel: () => void } {
  let timeoutId = 0;
  return {
    promise: new Promise((_, reject) => {
      timeoutId = window.setTimeout(() => {
        timeoutId = 0;
        reject(
          new Error(
            "Camera request timed out. Close other examples or apps using the camera, then restart this example."
          )
        );
      }, CAMERA_REQUEST_TIMEOUT_MS);
    }),
    cancel: () => {
      if (timeoutId !== 0) {
        window.clearTimeout(timeoutId);
        timeoutId = 0;
      }
    }
  };
}

function requestCamera(width: number, height: number): Promise<MediaStream> {
  return navigator.mediaDevices.getUserMedia({
    video: {
      width: { ideal: width },
      height: { ideal: height }
    },
    audio: false
  });
}

export function hasVideoFrame(video: HTMLVideoElement): boolean {
  return video.readyState >= 2 && video.videoWidth > 0 && video.videoHeight > 0;
}

export function getVideoTrack(stream: MediaStream): MediaStreamTrack | null {
  const tracks = stream.getVideoTracks();
  if (tracks.length === 0) {
    return null;
  }
  return tracks[0];
}

function trackLabel(stream: MediaStream): string {
  const track = getVideoTrack(stream);
  if (!track) {
    return "No video track returned.";
  }
  const settings = track.getSettings();
  const size =
    settings.width && settings.height
      ? ` ${settings.width}x${settings.height}`
      : "";
  return `${track.label || "camera"}${size}`;
}

export function describeCameraState(video: HTMLVideoElement, stream: MediaStream): string {
  const track = getVideoTrack(stream);
  const trackState = track
    ? `track=${track.readyState} enabled=${track.enabled ? "yes" : "no"} muted=${track.muted ? "yes" : "no"}`
    : "track=none";
  return `video readyState=${video.readyState} size=${video.videoWidth}x${video.videoHeight} paused=${
    video.paused ? "yes" : "no"
  } time=${video.currentTime.toFixed(2)} ${trackState}`;
}

async function requestCameraWithTimeout(width: number, height: number): Promise<MediaStream> {
  let timedOut = false;
  const request = requestCamera(width, height);
  const timeout = cameraTimeout();

  request
    .then((lateStream) => {
      if (timedOut) {
        for (const track of lateStream.getTracks()) {
          track.stop();
        }
      }
    })
    .catch(() => {});

  try {
    return await Promise.race([
      request,
      timeout.promise.catch((error) => {
        timedOut = true;
        throw error;
      })
    ]);
  } finally {
    timeout.cancel();
  }
}

function waitForFirstFrame(video: HTMLVideoElement): Promise<boolean> {
  if (hasVideoFrame(video)) {
    return Promise.resolve(true);
  }

  return new Promise((resolve) => {
    let settled = false;
    const timeoutId = window.setTimeout(() => {
      finish(false);
    }, CAMERA_FIRST_FRAME_TIMEOUT_MS);

    const finish = (success: boolean) => {
      if (settled) {
        return;
      }
      settled = true;
      window.clearTimeout(timeoutId);
      video.removeEventListener("loadeddata", handleEvent);
      video.removeEventListener("canplay", handleEvent);
      video.removeEventListener("playing", handleEvent);
      video.removeEventListener("timeupdate", handleEvent);
      video.removeEventListener("resize", handleEvent);
      resolve(success);
    };

    const handleEvent = () => {
      if (hasVideoFrame(video)) {
        finish(true);
      }
    };

    video.addEventListener("loadeddata", handleEvent);
    video.addEventListener("canplay", handleEvent);
    video.addEventListener("playing", handleEvent);
    video.addEventListener("timeupdate", handleEvent);
    video.addEventListener("resize", handleEvent);

    const requestVideoFrameCallback = (
      video as HTMLVideoElement & {
        requestVideoFrameCallback?: (callback: () => void) => number;
      }
    ).requestVideoFrameCallback;
    if (requestVideoFrameCallback) {
      requestVideoFrameCallback.call(video, () => finish(true));
    }
  });
}

function installDiagnostics(
  video: HTMLVideoElement,
  stream: MediaStream,
  onStage?: CameraStageReporter
): void {
  const reportEvent = (name: string) => {
    report(onStage, `camera_video_${name}`, `Camera video ${name}`, describeCameraState(video, stream));
  };

  for (const name of [
    "loadedmetadata",
    "loadeddata",
    "canplay",
    "play",
    "playing",
    "resize",
    "waiting",
    "stalled",
    "suspend",
    "error"
  ]) {
    video.addEventListener(name, () => reportEvent(name));
  }

  const track = getVideoTrack(stream);
  if (track) {
    track.addEventListener("mute", () => report(onStage, "camera_track_mute", "Camera track mute", describeCameraState(video, stream)));
    track.addEventListener("unmute", () => report(onStage, "camera_track_unmute", "Camera track unmute", describeCameraState(video, stream)));
    track.addEventListener("ended", () => report(onStage, "camera_track_ended", "Camera track ended", describeCameraState(video, stream)));
  }
}

async function waitForPlay(video: HTMLVideoElement): Promise<"playing" | "timeout"> {
  const playPromise = video.play();
  let timeoutId = 0;
  try {
    return await Promise.race([
      playPromise.then(() => "playing" as const),
      new Promise<"timeout">((resolve) => {
        timeoutId = window.setTimeout(() => {
          resolve("timeout");
        }, CAMERA_PLAY_TIMEOUT_MS);
      })
    ]);
  } catch (error) {
    throw error;
  } finally {
    if (timeoutId !== 0) {
      window.clearTimeout(timeoutId);
    }
    playPromise.catch(() => {});
  }
}

export async function startCamera(
  video: HTMLVideoElement,
  width: number,
  height: number,
  onStage?: CameraStageReporter
): Promise<CameraStartResult> {
  report(onStage, "camera_request", "Requesting camera", "Waiting for getUserMedia permission and device stream.");
  const stream = await requestCameraWithTimeout(width, height);
  report(onStage, "camera_stream", "Camera stream acquired", trackLabel(stream));
  installDiagnostics(video, stream, onStage);

  video.muted = true;
  video.autoplay = true;
  video.playsInline = true;
  video.setAttribute("muted", "");
  video.setAttribute("autoplay", "");
  video.setAttribute("playsinline", "");
  video.width = width;
  video.height = height;
  video.srcObject = stream;

  const playState = await waitForPlay(video).catch((error) => {
    const reason = error instanceof Error ? error.message : "video.play() failed";
    report(onStage, "camera_play_error", "Camera play error", `${reason}; ${describeCameraState(video, stream)}`);
    return "timeout" as const;
  });
  if (playState === "timeout") {
    report(onStage, "camera_play_pending", "Camera play pending", describeCameraState(video, stream));
    return { firstFrameReady: false, stream };
  }
  report(onStage, "camera_playing", "Camera playing", describeCameraState(video, stream));

  const firstFrameReady = await waitForFirstFrame(video);
  if (firstFrameReady) {
    report(
      onStage,
      "camera_ready",
      "Camera active",
      `Video frames ready: ${video.videoWidth || width}x${video.videoHeight || height}.`
    );
    return { firstFrameReady: true, stream };
  } else {
    report(onStage, "camera_waiting_first_frame", "Camera stream live", describeCameraState(video, stream));
    return { firstFrameReady: false, stream };
  }
}
