import { useEffect, useRef, useState, useCallback } from "react";
import { api, Status, Person } from "./api";
import Preview from "./components/Preview";
import WatchPanel from "./components/WatchPanel";
import EnrollPanel from "./components/EnrollPanel";
import PeoplePanel from "./components/PeoplePanel";

export default function App() {
  const [status, setStatus] = useState<Status | null>(null);
  const [people, setPeople] = useState<Person[]>([]);
  const lastAnnounce = useRef<number>(0);
  const initialized = useRef(false);

  const refreshPeople = useCallback(() => {
    api.people().then(setPeople).catch(() => {});
  }, []);

  // Poll server status ~2x/sec.
  useEffect(() => {
    let alive = true;
    const tick = () =>
      api.status().then((s) => alive && setStatus(s)).catch(() => {});
    tick();
    const id = setInterval(tick, 500);
    return () => { alive = false; clearInterval(id); };
  }, []);

  useEffect(refreshPeople, [refreshPeople]);

  // Speak a new announcement via the browser (Web Speech API).
  useEffect(() => {
    if (!status) return;
    const a = status.announce;
    if (!initialized.current) {
      initialized.current = true;
      lastAnnounce.current = a.id;
      return;
    }
    if (a.id > lastAnnounce.current) {
      lastAnnounce.current = a.id;
      if (!status.muted && a.name) {
        const phrase =
          "Possible match: " + a.name + (a.reminder ? ", " + a.reminder : "");
        try {
          window.speechSynthesis.cancel();
          window.speechSynthesis.speak(new SpeechSynthesisUtterance(phrase));
        } catch { /* speech not available */ }
      }
    }
  }, [status]);

  const toggleMute = () => status && api.setMute(!status.muted);

  return (
    <div className="app">
      <header className="topbar">
        <h1>Familiar Face Assistant</h1>
        <div className="topbar-right">
          <span className={"cam-dot " + (status?.cameraOpen ? "on" : "off")}>
            {status?.cameraOpen ? "● camera on" : "○ camera off"}
          </span>
          <button className="mute" onClick={toggleMute}>
            {status?.muted ? "🔇 Muted" : "🔊 Sound on"}
          </button>
        </div>
      </header>

      <main className="layout">
        <section className="left">
          <Preview cameraOpen={!!status?.cameraOpen} />
        </section>

        <section className="right">
          <WatchPanel status={status} />
          <EnrollPanel status={status} onChange={refreshPeople} />
          <PeoplePanel
            people={people}
            onChange={refreshPeople}
            disabled={status?.mode === "enroll"}
          />
        </section>
      </main>

      <footer className="foot">
        Local-only · nothing leaves this machine · a recognition aid, not a
        medical device.
      </footer>
    </div>
  );
}
