-- WHERE clause needs to be dynamically built once
-- can be used as prepared statemtn to fill in values afterwards
SELECT index FROM songs 
WHERE
  artist = ?,
  album  = ?
  -- and so forth.
;
