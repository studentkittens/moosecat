Overview
========

**mpd/**

	Contains all Features related to MPD.

**store/**

	Contains an implementation of a Song-Cache, using SQLite3.
	Uses mc_Client from mpd/, but mpd/ does not depend on store/.

**misc/**

	Misc features (like bug_reports, catching signals)

**util/**

	Shared code usefull for all modules.

Other notable files:

* **api.h:** Include this file to use libmooscat
* **compiler.h:** Special compiler directives (portable)
* **config.h:** Created in the ``waf configure`` process. Do not edit.
* **db_test.c:** Weird DB test program. Quite useful for debuggin.
* **event_test.c:** Example of using the event system.
* **gtk_db.c:** GTK2/3 TreeView containing the DB View. Nice for profiling.
