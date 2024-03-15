Nothing to see here.

Big swaths of work until there is though

- Format Support
    - SFZ Ranges. Really I need to visualize the jobs since I think they are laid out wrong
    - Is the inst block correct? Endpoint check etc
    - metadata?
- Changes
    - RR vars can be UNI or BI per instance and have a distro uni or norm
- UI
    - Show the keyboard as a 'progress bar'
    - Dropdown item
    - Spacing, labels not centered, etc in the control region
    - Recording Status display
    - Light skin doesn't suck
    - Scroll bar on the log would be nice eh
    - Status region next to buttons
    - Buttons get glyphs not text
    - Path regino shows path and has a set button rather than just massive button
    - An actual design! Lets hope Steve bites :)
- Important Details
    - Any error handling at all in the RIFF writer
    - Try/Catch around all filesystem things
    - A way to display errors on screen in the log as red text
    - store the path used in the patch
- Engine / Rendering
    - "Drone Mode" - what is it and how would it work?. I like the idea of cut in
      half and add fades but controlling that is tricky
    - Loop Points - should we do something?
    - A "Latency Mode" for when resampling host
    - Optionally render polyphonically to save time?
    - LintBuddy it
