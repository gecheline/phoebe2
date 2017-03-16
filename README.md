PHOEBE 2.0 RELEASE NOTES
------------------------

Hello and thank you for your interest in PHOEBE 2.0! PHOEBE is a binary star modeling code, but version 2.0 also supports the modeling of single rotating stars.


INTRODUCTION
------------

PHOEBE stands for PHysics Of Eclipsing BinariEs. PHOEBE is pronounced [fee-bee](https://www.merriam-webster.com/dictionary/phoebe?pronunciation&lang=en_us&file=phoebe01.wav).

PHOEBE 2.0 is a rewrite of the original PHOEBE code. For most up-to-date information please refer to the PHOEBE project webpage: [http://phoebe-project.org](http://phoebe-project.org)

PHOEBE 2.0 is described by the release paper published in the Astrophysical Journal Supplement, [Prša et al. (2016, ApJS 227, 29)](http://adsabs.harvard.edu/abs/2016ApJS..227...29P).

PHOEBE 2.0 is released under the [GNU General Public License v3](https://www.gnu.org/licenses/gpl-3.0.en.html).


The source code is available for download from the [PHOEBE project homepage](http://phoebe-project.org) and from [github](https://github.com/phoebe-project/phoebe2).

The development of PHOEBE 2.0 is funded in part by the [NSF grant #1517474](https://www.nsf.gov/awardsearch/showAward?AWD_ID=1517474).


DOWNLOAD AND INSTALLATION
-------------------------

The easiest way to download and install PHOEBE 2.0 is by using `pip`:

    `pip install phoebe`

To install it site-wide, prefix the `pip` command with `sudo` or run it as root.

To download the PHOEBE 2.0 source code, use git:

    `git clone https://github.com/phoebe-project/phoebe2.git`

To install PHOEBE 2.0 from the source locally, go to the `phoebe2/` directory and issue:

    `python setup.py build
    python setup.py install --user`

To install PHOEBE 2.0 from the source site-wide, go to the `phoebe2/` directory and issue:

    `python setup.py build
    sudo python setup.py install`

This will install PHOEBE 2.0 site-wide.

For further details on pre-requisites and minimal versions of python consult the PHOEBE webpage.


GETTING STARTED
---------------

PHOEBE 2.0 has a steep learning curve associated with it. There is
no graphical front-end as of yet; the front-end is now written in
python. To start PHOEBE, issue:

    python
    >>> import phoebe
    >>>

To understand how to use PHOEBE, please consult the tutorials, scripts
and manuals hosted on the PHOEBE webpage:

    http://phoebe-project.org/docs/2.0b/#Tutorials


CHANGELOG
----------

### 2.0 release

* PHOEBE 2.0 is not backwards compatible with PHOEBE 2.0-beta (although the
interface has not changed much at all) or with PHOEBE 2.0-alpha (complete
rewrite).  Going forward with incremental releases, this changelog will list
any necessary considerations when upgrading to a new version.

* If upgrading from PHOEBE 2.0-beta or PHOEBE 2.0-alpha, it is necessary to
do a clean re-install (clear your build and installation directories), as the
passband file format has changed and will not automatically reset unless these
directories are manually cleared.  Contact us with any problems.


QUESTIONS? SUGGESTIONS? CONCERNS?
---------------------------------

Contact us! Issues and feature requests should be submitted directly through
GitHub's issue tracker.  Two mailing lists are dedicated for discussion, either
on user level (phoebe-discuss@lists.sourceforge.net) or on the developer level
(phoebe-devel@lists.sourceforge.net). We are eager to hear from you, so do not
hesitate to contact us!
