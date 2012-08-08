Plugins
-------

* Plugin registriert sich an core mit:
  - Name
  - Liste von zur Verfügung gestellter Schnittstellen.
  - Version
  - ...
* Core überprüft ob alle angegebenen Schnittstellen vorhanden (hasattr())
* Core lädt das plugin via __import__(name)
* Core legt plugin in einem dictionary ab:

  {
      'metadata': {'glyr': <module object>, 'glyr2': <module object>}
      'gtk_browser': [...]
  }

* Andere Plugins können zB. folgendermaßen drauf zugreifen:

  # Evtl. shortcut möglich.
  core.m['metadata'][0].find_my_metadata(...)

* Falls ein Plugin abschmiert wird das obige dict gelockt, und das plugin wird removed.
  -> Calls werden an ein dummy object weitergeleitet.

* Mögliche Schnittstellen:

  - gtk_browser, qt_browser, nc_browser
  - metadata
  - cfgvalidator
  - gtk_cfgview, qt_cfgview 
  - ... werden wir sehen ...
