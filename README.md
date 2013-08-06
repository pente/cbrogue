cbrogue
=======

Community Brogue

How to set up the development environment
-----------------------------------------
* Install git
* Prepare the repository:

        mkdir cbrogue
        cd cbrogue
        git init
        git remote add origin https://github.com/pente/cbrogue.git
        git pull origin master

Updating your development environment
-------------------------------------
* To update to the newest version of the code:

        git pull origin master

* To checkout a specific version of the code to your working tree:

        git checkout <version>

* To restore your working tree to the newest version:

        git checkout master

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

        git pull origin master

* If the merge resulted in a merge conflict, you will have to resolve the
    conflict by hand and then commit your changes (after testing):

        git commit -a

* Finally you can push your changes to the public repository:

        git push origin master


How to compile
--------------
* Certain dependencies are required to be installed on your system to compile
    brogue. Check that you have installed `gcc` and the `rsvg`, `sdl-ttf` and
    `sdl` libraries. The packages you need on a Debian-style system are called:

        librsvg2-dev
        libsdl-ttf2.0-dev
        libsdl1.2-dev
* Once you have te requisite dependencies, run `make`.

How to run
----------
* Run `./brogue`.
