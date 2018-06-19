# DiskView
Command line tool to display hard disk or volume information

License: WTFPL

Requirements: Windows 7 or newer

Usage:

    DiskView.exe -d (index)                 
Display disk general information

    DiskView.exe -s (index)                 
Display disk S.M.A.R.T. attributes

    DiskView.exe -v (letter):               
Display volume information

    DiskView.exe -f (letter): (cluster)     
Find file by cluster number

    DiskView.exe -usn (letter): (year)-(month)-(day) (hour)-(min) (maxrecords)
Display up to (maxrecords) entries from volume's USN journal, search newer then specified date and time
