# DIST-UPDEMO-COLLISION evidence

Fail before (DIST_ROOT = main checkout, old parts.txt and old kit):
```
FAIL: test_built_kit_has_program_and_media (__main__.UpdemoNames.test_built_kit_has_program_and_media)
FAIL: test_parts_txt_has_no_case_insensitive_duplicates (__main__.UpdemoNames.test_parts_txt_has_no_case_insensitive_duplicates)
FAIL: test_parts_txt_places_the_media_under_share (__main__.UpdemoNames.test_parts_txt_places_the_media_under_share)
Ran 5 tests in 0.014s
FAILED (failures=3)
```
Pass after (real kit from make dist):
```
Ran 5 tests in 0.113s
OK
$ ls build/dist/up-term/Files | grep -i updemo
UPDemo
$ ls build/dist/up-term/Files/share/updemo
earth.anim
kanim-stream.bin
photo.png
```

dist-check finding: tools/mkmanifest.py absent() used Path.exists(), case-insensitive on APFS, so Files/UPDemo counted as present while only the drawer Files/updemo existed. Fixed with exact_exists() (listdir-based); tests/test_dist_updemo_names.py is now in dist-check.
