========================================================================
    КОНСОЛЬНОЕ ПРИЛОЖЕНИЕ. Обзор проекта DiskView
========================================================================

Project: DiskView 
Author: MSDN.WhiteKnight (https://github.com/MSDN-WhiteKnight)
License: WTFPL

Displays hard disk or volume information

Usage:

    DiskView.exe -d (index)                 
display disk general information

    DiskView.exe -s (index)                 
display disk S.M.A.R.T. attributes

    DiskView.exe -v (letter):               
display volume information

    DiskView.exe -f (letter): (cluster)     
find file by cluster number

    DiskView.exe -usn (letter): (year)-(month)-(day) (hour)-(min) (maxrecords)
Display up to (maxrecords) entries from volume's USN journal, search newer then specified date and time

/////////////////////////////////////////////////////////////////////////////
