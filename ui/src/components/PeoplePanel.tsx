import { api, Person } from "../api";

// Review & remove enrolled people. Deletion actually removes the data.
export default function PeoplePanel({
  people, onChange, disabled,
}: { people: Person[]; onChange: () => void; disabled: boolean }) {
  const remove = async (name: string) => {
    if (!confirm(`Delete "${name}" and their face data?`)) return;
    await api.deletePerson(name);
    onChange();
  };
  const removeAll = async () => {
    if (!confirm("Delete ALL enrolled people and their face data?")) return;
    await api.deleteAll();
    onChange();
  };

  return (
    <div className="panel">
      <div className="panel-head">
        <h2>Enrolled people ({people.length})</h2>
        {people.length > 0 && (
          <button className="danger small" onClick={removeAll} disabled={disabled}>
            Delete all
          </button>
        )}
      </div>
      {people.length === 0 ? (
        <p className="hint">No one enrolled yet.</p>
      ) : (
        <ul className="people">
          {people.map((p) => (
            <li key={p.id}>
              <span>
                <b>{p.name}</b>
                {p.reminder && <span className="reminder"> · {p.reminder}</span>}
                <span className="count"> · {p.embeddings} shots</span>
              </span>
              <button className="ghost small" onClick={() => remove(p.name)} disabled={disabled}>
                Delete
              </button>
            </li>
          ))}
        </ul>
      )}
    </div>
  );
}
