# Move home directory on Windows

Moving the home directory is not a trivial task on Windows.

Even if we move the directory itself with another user the registry has our directory hardcoded in quite a few places.

I wrote a C++ utility, which replaces all entries pointing at the previous home directory to the new one.

This utility enhances the following answer on stackoverflow:
* https://superuser.com/questions/1190364/how-to-set-home-directory-in-win10

That answer in turn incorporates the following articles:
* https://www.lifewire.com/how-to-find-a-users-security-identifier-sid-in-windows-2625149
* https://www.sevenforums.com/tutorials/87555-user-profile-change-default-location.html

Also note, these articles forget to mention that a user's directory should be owned by the SYSTEM user, but all subfolders and files should be given "Total control" for the user whos directory it is. In case this is not the case even the start menu doesn't work.
