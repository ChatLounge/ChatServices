## ChatServices

ChatServices is a fork of the Atheme services software.

Like Atheme, ChatServices is a set of services for IRC networks designed for large IRC
networks with high scalability requirements.  Some code and design has been derived
from another package called Shrike.

ChatServices's behavior is tunable using modules and a highly detailed configuration file.
Almost all behavior can be changed at deployment time just by editing the configuration.

## Obtaining ChatServices
You may use git clone on https://bitbucket.org/chatlounge/chatservices.git
While tarballs may be available, git is preferred since you will only download the actual
code changes.
-- Please do not click the Download buttons
on GitHub as they lack needed submodules, etc.

If you are running this code from Git, you should read GIT-Access for instructions on
how to fully check out the ChatServices tree, as it is spread across many repositories.

## Basic build instructions for the impatient

Whatever you do, make sure you do *not* install ChatServices into the same location as the source.
ChatServices will default to installing in `$HOME/services`, so make sure you plan accordingly for this.
By default if you git clone, it will download to `chatservices`, avoiding this conflict.

    $ git submodule update --init
    $ ./configure --options-you-want-here
    $ make
    $ make install

If you're still lost, read [INSTALL](INSTALL) or [GIT-Access](GIT-Access) for hints.

## links / contact

 * [Git](https://bitbucket.org/chatlounge/chatservices/)
 * [Website](https://www.chatlounge.net)
 * [IRC](ircs://irc.chatlounge.net/#ChatServices)

 #ChatServices @ irc.chatlounge.net

NOTE:
Please don't ask the Atheme Development Group for assistance with this software.
