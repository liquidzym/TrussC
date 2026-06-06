# ios-background-download

TrussC iOS example for `tcxIOS` BackgroundTasks and background URLSession downloads.

For real device testing with `BGTaskScheduler`, add the identifier below to `BGTaskSchedulerPermittedIdentifiers` in the app Info.plist:

- `com.trussc.tcxios.example.refresh`

Background scheduling is system controlled and may not run immediately.
