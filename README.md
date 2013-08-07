cbrogue
=======

Community Brogue (tcod-based version)

How to set up the development environment
-----------------------------------------
* Install git
* Prepare the repository:

        mkdir cbrogue
        cd cbrogue
        git init
        git remote add origin https://github.com/pente/cbrogue.git
        git pull origin tcod

Updating your development environment
-------------------------------------
* To update to the newest version of the code:

        git pull origin tcod

Contributing edits
------------------
* Create a github account and set up your user name and email.
* Get permission for your github account to make edits to the public repository.
* After you have made some edits to the code and tested thoroughly that your
    new code works, commit your changes to git:
        
        git commit -a -m 'Message describing the changes you made'

* Make sure you have the newest version before pushing your changes. If you did
    not have the latest version, git will automatically merge your code with the
    latest code. Be sure to test the code after performing any merges.

        git pull origin tcod

* If the merge resulted in a merge conflict, you will have to resolve the
    conflict by hand and then commit your changes (after testing):

        git commit -a

* Finally you can push your changes to the public repository:

        git push origin tcod


How to compile
--------------
* The first time you compile Brogue, you must do the following to obtain the
    tcod development library:

        cd src
        ./get-libtcod.sh
        cd ..

* Then every time you need to compile Brogue, execute:

        make

* (TODO: dependencies)

How to run
----------
* Run `./brogue`
