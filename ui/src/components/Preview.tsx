// Live camera preview. The C++ server streams an annotated MJPEG; we just show
// it in an <img>. The stream URL is loaded once so the browser keeps the
// long-lived multipart connection open.

export default function Preview({ cameraOpen }: { cameraOpen: boolean }) {
  return (
    <div className="preview">
      <img className="preview-img" src="/stream.mjpg" alt="camera preview" />
      {!cameraOpen && (
        <div className="preview-warn">
          Camera not available. Grant camera access to your terminal/app and make
          sure no other app is using the webcam.
        </div>
      )}
    </div>
  );
}
