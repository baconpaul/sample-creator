Nothing to see here.

Big swaths of work until there is though

- Format Support
  - Is the inst block correct? Endpoint check etc
  - bws/presonus multisample xml
  - directory structure shifts to "foo/x.sfz", "foo/x.multisample" and "foo/wav/files"
- Changes
  - RR vars can be UNI or BI per instance and have a distro uni or norm 
- UI
  - Draw the damn thing once on paper to kinda get an idea 
  - Show the keyboard as a 'progress bar'
  - Note param quantity inverts from note names too
  - Recording Status
  - An actual design! Lets hope Steve bites :)
- Important Details
  - Any error handling at all in the RIFF writer 
- Engine / Rendering
  - "Drone Mode" - what is it and how would it work?. I like the idea of cut in 
half and add fades but controlling that is tricky
  - Loop Points - should we do something?
  - A "Latency Mode" for when resampling host
  - Optionally render polyphonically to save time?
