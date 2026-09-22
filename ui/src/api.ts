// Typed client for the local C++ server (127.0.0.1). Pure presentation layer —
// all recognition happens on the server.

export type Mode = "idle" | "watch" | "enroll";

export interface Status {
  mode: Mode;
  cameraOpen: boolean;
  muted: boolean;
  decision: "idle" | "match" | "unknown" | "unsure" | "lowquality";
  name: string;
  reminder: string;
  score: number;
  enroll: { name: string; count: number; ready: boolean; reason: string };
  announce: { id: number; name: string; reminder: string };
}

export interface Person {
  id: number;
  name: string;
  reminder: string;
  embeddings: number;
}

async function post(path: string, body?: unknown) {
  const res = await fetch(path, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: body ? JSON.stringify(body) : undefined,
  });
  return res.json();
}

export const api = {
  status: (): Promise<Status> => fetch("/api/status").then((r) => r.json()),
  people: (): Promise<Person[]> => fetch("/api/people").then((r) => r.json()),
  enrollStart: (name: string, reminder: string) =>
    post("/api/enroll/start", { name, reminder }),
  enrollCapture: (): Promise<{ ok: boolean; reason: string; count: number }> =>
    post("/api/enroll/capture"),
  enrollFinish: (): Promise<{ ok: boolean; count: number }> =>
    post("/api/enroll/finish"),
  watchStart: () => post("/api/watch/start"),
  watchStop: () => post("/api/watch/stop"),
  setMute: (muted: boolean) => post("/api/mute", { muted }),
  deletePerson: (name: string) => post("/api/people/delete", { name }),
  deleteAll: () => post("/api/people/deleteAll"),
};
