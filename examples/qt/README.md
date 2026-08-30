# Qt / QML integration

Keep the Quilibrium SDK in the C++ application layer and expose presentation-ready models/controllers to QML. `FeedController` is intentionally small: the blocking default curl transport runs on a QtConcurrent worker and the result is marshalled back to the GUI thread.

For a production Qt client, keep one long-lived SDK/service object in your application service layer rather than constructing it for every request. You can also inject a Qt-native HTTP transport if you want all network I/O integrated with the Qt event loop.

The example assumes a context property named `feedController` and links Qt Core, Quick, QuickControls2, and Concurrent plus `Quilibrium::SDK`.
