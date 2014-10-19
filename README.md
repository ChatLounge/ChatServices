## ChatServices

ChatServices is a fork of the now defunct Atheme services software.

Atheme was a set of services for IRC networks designed for large IRC networks with high
scalability requirements.  It was relatively mature software, with some code and design
derived from another package called Shrike.

ChatServices's behavior is tunable using modules and a highly detailed configuration file.
Almost all behavior can be changed at deployment time just by editing the configuration.

If you are running this code from Git, you should read GIT-Access for instructions on
how to fully check out the atheme tree, as it is spread across many repositories.

## basic build instructions for the impatient

Whatever you do, make sure you do *not* install ChatServices into the same location as the source.
ChatServices will default to installing in `$HOME/services`, so make sure you plan accordingly for this.

    $ git submodule update --init
    $ ./configure --options-you-want-here
    $ make
    $ make install

If you're still lost, read [INSTALL](INSTALL) or [GIT-Access](GIT-Access) for hints.

## links / contact

 * [Git](https://bitbucket.org/chatlounge/chatservices/)
 * [Website](http://www.chatlounge.net)
 * [IRC](irc://irc.chatlounge.net/#ChatServices)

 #ChatServices @ irc.chatlounge.net

NOTE:
Please don't ask the Atheme Development Group for assistance with this software.
