# aimpatch

This DLL removes the big ad at the bottom of the new AIM 7.5 window.

It does this by simply finding the main AIM window and hiding all of its direct children.

The DLL itself is loaded by pretending to be a normal DLL loaded by AIM (`jgtktlk.dll`), which happens to be one of the few which is loaded dynamically at runtime with `GetProcAddress` and does not have any resources. This DLL then loads the original DLL which has been renamed to `~jgtktlk.dll`.

In order to prevent the need for this DLL to export all of the functions that `~jgtktlk.dll` does and route the calls to the original DLL, we hook `GetProcAddress`. Every time `GetProcAddress` is called with this DLL as its target, the hook blocks it and calls `GetProcAddress` with the original DLL as its target, completely bypassing this DLL when functions in the original need to be called.

Incidentally, `jgtktlk.dll` is only loaded at sign-in, and is totally unloaded at sign-out, which obviously breaks things terribly with our `GetProcAddress` hook. I could have unloaded the `GetProcAddress` hook on `DLL_PROCESS_DETACH` as a simple fix, but I opted to hook `FreeLibrary` and block the call if the desired DLL to unload is this one. Blocking `FreeLibrary` is easier and keeps this DLL loaded the whole time which actually works out better, I think.

Finally, when this DLL is loaded, it simply starts a thread which first finds the AIM window and then continuously hides its children.

In short, getting this DLL loaded by default without the need for external DLL injectors took much more work than hiding the ad.
