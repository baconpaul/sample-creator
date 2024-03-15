Nothing to see here.

Big swaths of work until there is though

- Format Support
    - Is the inst block correct? Endpoint check etc
    - other sample metadata?
- UI
    - Round Robin overpaint in keyboard
    - Double buffer keyboard
    - Recording Status display
    - Light skin doesn't suck (and is objeyed in step() in the Skin)
    - Scroll bar on the log would be nice eh
    - Maybe timestamp in the scroll bar?
    - Status region next to buttons
    - Buttons get glyphs not text
    - Path regino shows path and has a set button rather than just massive button
    - More instructive log messages that fit the screen region given
    - An actual design! Lets hope Steve bites :)
- Important Details
    - Any error handling at all in the RIFF writer
    - Try/Catch around all filesystem things
    - A way to display errors on screen in the log as red text
    - store the path used in the patch
- Engine / Rendering
    - Impl RR strategy
    - "Drone Mode" - what is it and how would it work?. I like the idea of cut in
      half and add fades but controlling that is tricky
    - Loop Points - should we do something?
    - A "Latency Mode" for when resampling host
    - Optionally render polyphonically to save time?
  