# Privacy & Safety

This project handles **biometric data** (face templates of real people). That
carries real responsibility. These are the rules the code is built to keep.

## Consent
- Enrollment is **opt-in only**. Enroll a person only with their informed
  permission. The app creates and stores a face template ("embedding") for them.
- Face embeddings are **not anonymous**. They are derived from a person's face
  and are treated as sensitive personal/biometric data.

## Local-only
- All face images, embeddings, names, and processing stay **on this machine**.
- The app does **not** send frames or data to any AI API, analytics service, or
  remote server at runtime.
- The (later) local API binds to `127.0.0.1` only — never exposed to the network.
- The only network use is a **one-time build step** (downloading libraries and
  the open-source face models), which downloads code onto the machine and sends
  nothing out.

## No silent recording
- Routine camera footage and recognition history are **not** saved by default.
- Only what enrollment needs (a few embeddings, optional photos, a name) is stored.

## Deletion actually deletes
- **Delete-one** removes a person's name, reminder, embeddings, and photos.
- **Delete-all** wipes the entire local database and photo folder.

## Honest language
- The app says **"Possible match"**, not certainties.
- It never shows a similarity score as a confidence percentage unless that score
  has actually been calibrated (it has not).
- This is a **recognition aid**, not a medical device, diagnosis, treatment, or a
  substitute for the user's own judgment.
- It only compares against **enrolled** people — it cannot identify strangers.

## Camera indicator
- When the camera is active, the UI shows a clear "camera on" indicator.
