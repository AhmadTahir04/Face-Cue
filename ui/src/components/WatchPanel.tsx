import { api, Status } from "../api";

// Live recognition controls + the result card. The card deliberately shows
// "Possible match", "unsure", or "unknown" — never a % confidence.
export default function WatchPanel({ status }: { status: Status | null }) {
  const watching = status?.mode === "watch";
  const enrolling = status?.mode === "enroll";

  const card = () => {
    if (!watching) return { text: "Not watching", cls: "idle" };
    switch (status?.decision) {
      case "match":
        return {
          text: "Possible match: " + status.name +
            (status.reminder ? ` (${status.reminder})` : ""),
          cls: "match",
        };
      case "unsure": return { text: "Not sure", cls: "unsure" };
      case "lowquality": return { text: "Move closer / hold still", cls: "unsure" };
      default: return { text: "Unknown", cls: "unknown" };
    }
  };
  const c = card();

  return (
    <div className="panel">
      <h2>Recognize</h2>
      <div className={"result-card " + c.cls}>{c.text}</div>
      {!watching ? (
        <button
          className="primary"
          disabled={enrolling}
          onClick={() => api.watchStart()}
          title={enrolling ? "Finish enrollment first" : ""}
        >
          Start watching
        </button>
      ) : (
        <button className="danger" onClick={() => api.watchStop()}>
          Stop watching
        </button>
      )}
      <p className="hint">
        Watches hands-free and announces an enrolled person once, then stays
        quiet. Silent for unknown faces.
      </p>
    </div>
  );
}
