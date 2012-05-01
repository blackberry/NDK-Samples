VideoWindow

========================================================================
Sample Description:

 The VideoWindow sample is an application that is designed to show you how to
 use the multimedia capabilities of the Native SDK in concert with OpenGL ES
 2.0.

 When the application starts, there is a green "Play" symbol that can be
 pressed.

 When the button is pressed, the play button turns into a blue stop symbol and
 video playback begins.  While the video is playing, the user can chose to stop
 the video by tapping the blue symbol.  The blue stop symbol will then turn into
 a green play symbol.  When the green symbol is pressed, playback will continue.

 The user may swipe from the top of the bevel into the video (down-swipe) in
 order to hide the symbol.  This is achieved by changing the z-order of the
 mmrender video surface.

 Feature summary
 - Creating a playback window
 - Connecting and configuring the renderer
 - Configuring video input and output
 - Starting and stopping video playback
 - OpenGL ES 2.0 graphics composited on top of video

========================================================================
Requirements:

 - BlackBerry® 10 Native SDK
 - One of the following:
   - BlackBerry® 10 device
   - BlackBerry® 10 simulator

========================================================================
Importing a project into the Native SDK:

 1. From the the Sample apps page, download and extract the sample application.
 2. Launch the Native SDK.
 3. On the File menu, click Import.
 4. Expand General, and select Existing Projects into Workspace. Click Next.
 5. Browse to the location where you extracted the sample app, and click OK.
    The sample project should display in the the Projects section.
 6. Click Finish to import the project into your workspace.

