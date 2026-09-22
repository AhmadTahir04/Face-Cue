// Live camera preview. The C++ server streams an annotated MJPEG; we show it in
// an <img>. `streamNonce` changes whenever the camera comes (back) online, which
// remounts the <img> so it reconnects to the fresh stream — e.g. after the
// server restarts, without needing a manual page reload.

export default function Preview({
  cameraOpen, streamNonce,
}: { cameraOpen: boolean; streamNonce: number }) {
  return (
    <div className="preview">
      <img
        key={streamNonce}
        className="preview-img"
        src={`/stream.mjpg?n=${streamNonce}`}
        alt="camera preview"
      />
      {!cameraOpen && (
        <div className="preview-warn">
          Camera not available. Grant camera access to Terminal, disable your
          iPhone's Continuity Camera if it grabbed the wrong device, and make sure
          no other app is using the webcam.
        </div>
      )}
    </div>
  );
}
