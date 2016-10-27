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
|---------|-------|---------------|-------------------------------------------------------------|
| list    | l     | [pageNo]      | List search results.                                        |
| search  | s     | [searchVal]   | First search & Search again with [searchVal].               |
| fuzy    | f     | [compareType] | First fuzzy search & Fuzzy search again with [compareType]. |
| reset   | r     | -             | Reset the search state.                                     |
| retype  | -     | -             | Re-select the search value type.                            |
| exit    | -     | -             | Exit the program.                                           |