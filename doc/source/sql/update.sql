-- On DB_UPDATE event:
-- if statistics.db_version == meta.db_version:
--    pass # Nothing needs to be done
-- else:

DELETE FROM TABLE songs;

-- Drop indices.
DROP INDEX...;

-- Now reinitialize. 