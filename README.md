Nothing to see here.

Big swaths of work until there is though

- Format Support
    - Do we want to store or write other sample metadata?
    - Bitwig Sampler Format
- UI
    - Double buffer keyboard
    - Light skin doesn't suck (and is objeyed in step() in the Skin)
    - Scroll bar on the log would be nice eh
    - Buttons get glyphs not text
    - Path regino shows path better and has a better set button
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
    - Implement "Latency Mode" for when resampling host
    - Implement polyphonic rendering
    - Test then Start without a Stop gets the state confused
    - SFZ won't load in tal sampler. Why?
