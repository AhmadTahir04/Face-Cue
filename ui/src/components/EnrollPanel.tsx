import { useState } from "react";
import { api, Status } from "../api";

// Enrollment flow. The server owns the camera, so "Capture" tells the server to
// grab the current frame, quality-check it, and store one embedding.
export default function EnrollPanel({
  status, onChange,
}: { status: Status | null; onChange: () => void }) {
  const [name, setName] = useState("");
  const [reminder, setReminder] = useState("");
  const [busy, setBusy] = useState(false);
  const [msg, setMsg] = useState("");

  const enrolling = status?.mode === "enroll";
  const watching = status?.mode === "watch";
  const count = status?.enroll.count ?? 0;
  const ready = status?.enroll.ready ?? false;

  const start = async () => {
    if (!name.trim()) return;
    await api.enrollStart(name.trim(), reminder.trim());
    setMsg("Look at the camera and capture a few shots from slightly different angles.");
  };
  const capture = async () => {
    setBusy(true);
    const r = await api.enrollCapture();
    setMsg(r.ok ? `Captured ${r.count} shot(s).` : `Skipped: ${r.reason}`);
    setBusy(false);
  };
  const finish = async () => {
    const r = await api.enrollFinish();
    setMsg(`Enrolled "${name}" with ${r.count} shot(s).`);
    setName(""); setReminder("");
    onChange();
  };

  return (
    <div className="panel">
      <h2>Enroll a person</h2>
      <p className="consent">
        Only enroll people who have agreed. This stores a face template for them.
      </p>

      {!enrolling ? (
        <div className="form">
          <input
            placeholder="Name (e.g. Sarah)"
            value={name}
            onChange={(e) => setName(e.target.value)}
            disabled={watching}
          />
          <input
            placeholder="Reminder (optional, e.g. cousin)"
            value={reminder}
            onChange={(e) => setReminder(e.target.value)}
            disabled={watching}
          />
          <button
            className="primary"
            onClick={start}
            disabled={watching || !name.trim()}
            title={watching ? "Stop watching first" : ""}
          >
            Start enrollment
          </button>
        </div>
      ) : (
        <div className="enroll-active">
          <div className="enroll-status">
            Enrolling <b>{status?.enroll.name}</b> · captured <b>{count}</b>
            <span className={"ready-pill " + (ready ? "ok" : "no")}>
              {ready ? "face ready" : status?.enroll.reason || "no face"}
            </span>
          </div>
          <div className="row">
            <button className="primary" onClick={capture} disabled={busy || !ready}>
              Capture shot
            </button>
            <button className="ghost" onClick={finish}>
              Finish
            </button>
          </div>
          <p className="hint">Aim for 5+ shots: straight on, slight left, slight right.</p>
        </div>
      )}
      {msg && <p className="msg">{msg}</p>}
    </div>
  );
}
