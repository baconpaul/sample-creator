Nothing to see here.

Big swaths of work until there is though

- Format Support
    - SFZ Ranges. Really I need to visualize the jobs since I think they are laid out wrong
    - Is the inst block correct? Endpoint check etc
    - metadata?
- Changes
    - RR vars can be UNI or BI per instance and have a distro uni or norm
    - add a trigger output and the out section can be 4x4 of voct/vel/gate/trig rr/mode/rr/mode/rri
    - Where rri is round robin index as a 0-10 percent
- UI
    - Draw the damn thing once on paper to kinda get an idea
    - Show the keyboard as a 'progress bar'
    - Recording Status
    - An actual design! Lets hope Steve bites :)
    - Light skin doesn't suck
    - Scroll bar on the log would be nice eh
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
