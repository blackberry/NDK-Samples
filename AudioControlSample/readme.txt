Audio control - Determine audio control settings

========================================================================
Using a managed project vs. using a makefile project

 This package contains two different types of project options. One is an eclipse-based 
 managed project and the other is a makefile project. The managed project leverages 
 Eclipse to manage the build process so that you don’t have to worry about makefiles. 
 When using this option you can adjust build settings through the project properties. 
 The makefile project allows you to see and edit makefiles for direct control of the 
 build process. 

========================================================================
Sample Description:
 
 The Audio Control Sample is an application that is designed to show you how to
 query the device for audio control settings, such as headphone volume,
 speaker volume, input gain, and mute status. This sample uses the audio mixer
 service, which is a service in the BlackBerry Platform Services library.
 
 When you run the application, you can change one of the properties of the audio
 mixer and the updated audio mixer status is printed to the console.
 
 Feature summary
 - Handling audio mixer events
 - Querying the audio mixer service

========================================================================
Requirements:

 - BlackBerry Native SDK for Tablet OS 1.0 or later
 - One of the following:
   - BlackBerry PlayBook tablet running BlackBerry Tablet OS 1.0 or later  
   - BlackBerry Tablet Simulator 1.0 or later
 
========================================================================
Importing a project into the Native SDK:
   
 1. From the the Sample apps page, download and extract the sample application.
 2. Launch the Native SDK.
 3. On the File menu, click Import.
 4. Expand General, and select Existing Projects into Workspace. Click Next.
 5. Browse to the location where you extracted the sample app, and click OK.
    The sample project should display in the the Projects section. 
 6. Click Finish to import the project into your workspace.

