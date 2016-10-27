Requirments
===================
 - [theos](https://github.com/theos/theos)
 - readline

----------

Make & Install
-------------

    $ cd rxmemscan
    $ make do FINALPACKAGE=1

----------

Usage
-------------
    
 | Command | Short | Args          | Desc                                                        |
|---------|-------|---------------|-------------------------------------------------------------|                                       |
| search  | s     | [searchVal]   | First search & Search again with [searchVal].               |
| fuzzy    | f     | [compareType] | First fuzzy search & Fuzzy search again with [compareType]. |
| list    | l     | [pageNo]      | List search results. 
| reset   | r     | -             | Reset the search engine state.                                     |
| retype  | -     | -             | Re-select the search value type.                            |
| exit    | -     | -             | Exit the program.                                           |
