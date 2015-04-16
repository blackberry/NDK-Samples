Gamepad - Handle and configure game controller events

========================================================================
Sample Description:

The Gamepad sample is an application that demonstrates how to handle
gamepad and joystick events. It can handle two connected game controllers
at once.

When an HID game controller is connected to the device, a collection of
buttons and analog sticks representing the controller will be drawn.
This "virtual" gamepad will react to input events from the controller.
Tapping a virtual button on the screen, then pressing a physical button
on the gamepad, allows the buttons to be re-assigned -- much like a game
would allow players to change how it is controlled.

Feature summary:
- Discovery of gamepad and joystick devices which are already connnected.
- Handling of controller connect and disconnect events.
- Handling of controller input events.
- Configuration of controller buttons.

========================================================================
Requirements:

 - BlackBerry® 10 Native SDK
 - BlackBerry® 10 device

========================================================================
Importing a project into the Native SDK:

 1. From the the Sample apps page, download and extract the sample application.
 2. Launch the Native SDK.
 3. On the File menu, click Import.
 4. Expand General, and select Existing Projects into Workspace. Click Next.
 5. Browse to the location where you extracted the sample app, and click OK.
    The sample project should display in the the Projects section.
 6. Click Finish to import the project into your workspace.