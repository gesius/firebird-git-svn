/* MAX_NUMBER is the next number to be used, always one more than the highest message number. */
set bulk_insert INSERT INTO FACILITIES (LAST_CHANGE, FACILITY, FAC_CODE, MAX_NUMBER) VALUES (?, ?, ?, ?);
--
('2014-12-16 21:00:00', 'JRD', 0, 782)
('2012-01-23 20:10:30', 'QLI', 1, 532)
('2015-01-07 18:01:51', 'GFIX', 3, 134)
('1996-11-07 13:39:40', 'GPRE', 4, 1)
('2012-08-27 21:26:00', 'DSQL', 7, 33)
('2014-04-22 16:39:03', 'DYN', 8, 290)
('1996-11-07 13:39:40', 'INSTALL', 10, 1)
('1996-11-07 13:38:41', 'TEST', 11, 4)
('2014-05-09 01:30:36', 'GBAK', 12, 361)
('2014-05-02 19:19:51', 'SQLERR', 13, 1042)
('1996-11-07 13:38:42', 'SQLWARN', 14, 613)
('2006-09-10 03:04:31', 'JRD_BUGCHK', 15, 307)
('2014-05-07 03:04:46', 'ISQL', 17, 190)
('2010-07-10 10:50:30', 'GSEC', 18, 105)
('2015-01-07 18:01:51', 'GSTAT', 21, 58)
('2013-12-19 17:31:31', 'FBSVCMGR', 22, 58)
('2009-07-18 12:12:12', 'UTL', 23, 2)
('2015-01-07 18:01:51', 'NBACKUP', 24, 77)
('2009-07-20 07:55:48', 'FBTRACEMGR', 25, 41)
stop

COMMIT WORK;
