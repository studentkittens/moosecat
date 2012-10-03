
/*
 * Random thoughts about storing playlists:
 *
 * Create pl table:
 * CREATE TABLE spl_%q (song_idx integer, FOREIGN KEY(song_idx) REFERENCES songs(rowid));
 * 
 * Update a playlist:
 * BEGIN IMMEDIATE;
 * DELETE FROM spl_%q;
 * INSERT INTO spl_%q VALUES((SELECT rowid FROM songs_content WHERE c0uri = %Q)); 
 * ...
 * COMMIT;
 *
 * Select songs from playlist:
 * SELECT rowid FROM songs JOIN spl_test ON songs.rowid = spl_test.song_idx WHERE artist MATCH 'akr*';
 *  
 *
 * Delete playlist:
 * DROP spl_%q;
 *
 * List all loaded playlists:
 * SELECT name FROM sqlite_master WHERE type = 'table' AND name LIKE 'spl_%' ORDER BY name;
 *
 *
 * The plan:
 * On startup OR on MPD_IDLE_STORED_PLAYLIST:
 *    Get all playlists, either through:
 *        listplaylists
 *    or during 
 *        listallinfo
 *
 *    The mpd_playlist objects are stored in a stack.
 *
 *    Sync those with the database, i.e. playlists that are not up-to-date,
 *    or were deleted are dropped. New playlists are __not__ created.
 *    If the playlist was out of date it gets recreated.
 *   
 * On playlist load:
 *    Send 'listplaylist <pl_name>', create a table named spl_<pl_name>,
 *    and fill in all the song-ids
 */
