Nothing to see here.

Big swaths of work until there is though

- Format Support
    - Is the inst block correct? Endpoint check etc
    - bws/presonus multisample xml
    - directory structure shifts to "foo/x.sfz", "foo/x.multisample" and "foo/wav/files"
- Changes
    - RR vars can be UNI or BI per instance and have a distro uni or norm
    - add a trigger output and the out section can be 4x4 of voct/vel/gate/trig rr/mode/rr/mode
- UI
    - Draw the damn thing once on paper to kinda get an idea
    - Show the keyboard as a 'progress bar'
    - Recording Status
    - An actual design! Lets hope Steve bites :)
    - Light skin doesn't suck
- Important Details
    - Any error handling at all in the RIFF writer
    - Try/Catch around all filesystem things
    - A way to display errors on screen
    - stopre the path used in the patch
- Engine / Rendering
    - "Drone Mode" - what is it and how would it work?. I like the idea of cut in
      half and add fades but controlling that is tricky
    - Loop Points - should we do something?
    - A "Latency Mode" for when resampling host
    - Optionally render polyphonically to save time?
    - LintBuddy it
