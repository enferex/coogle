## coogle: cscope search utility

### What
coogle is a simple cscope search utility.  Input search expressions
into coogle, and it will report the file and line number for the matching
text.  Nothing fancy, no black magic or unicorn tears.  Just a simple cscope
database search utility.  You could just use cscope, but this is meant as a
quick goto utility that can be left open in a separate term.

### Depends
Depends on the readline library.  You have this... right? If not, you can snag
it from here: http://cnswww.cns.cwru.edu/php/chet/readline/rltop.html

To create a cscope database you will need cscope, which can be obtained here:
http://cscope.sourceforge.net/

### Build
Run `make'

### Run
The only argument is the cscope file. 

To build a cscope database run cscope with the '-b' option.  For example:
    cscope -b *.c

The latter command will search all .c files in your current working directory
and produce a cscope.out file.  That file, the cscope.out, is the cscope
database that coogle takes as input:
    coogle cscope.out

### Contact
Matt Davis (enferex)
mattdavis9@gmail.com
