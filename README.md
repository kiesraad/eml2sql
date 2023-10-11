# eml2sql
Convert Dutch election .EML files to a SQLite database with no loss of data

(C) 2023 Kiesraad

This is RESEARCH software that is not used in production.
Please do file issues [on the issue tracker](https://github.com/berthubert/eml2sql/issues), but do know that a bug in this code is not a bug in the Dutch elections!

# Compiling

Make sure you have installed sqlite3-dev and cmake, and then:

```bash
cmake .
make
```

# Data

 * [Source of EML data](https://data.overheid.nl/community/organization/kiesraad)
 
Election data comes as three zip files, which need to be unzipped so their
data ends up in one place. This is a bit tricky, but try:

```bash
wget https://data.overheid.nl/sites/default/files/dataset/be8b7869-4a12-4446-abab-5cd0a436dc4f/resources/EML_bestanden_PS2023_deel_1.zip
wget https://data.overheid.nl/sites/default/files/dataset/be8b7869-4a12-4446-abab-5cd0a436dc4f/resources/EML_bestanden_PS2023_deel_2.zip
wget https://data.overheid.nl/sites/default/files/dataset/be8b7869-4a12-4446-abab-5cd0a436dc4f/resources/EML_bestanden_PS2023_deel_3.zip
unzip EML_bestanden_PS2023_deel_1.zip
chmod -R u+rwX  EML_bestanden_PS2023_deel_1
mv EML_bestanden_PS2023_deel_1 EML_bestanden_PS2023_deel_2
unzip EML_bestanden_PS2023_deel_2.zip
chmod -R u+rwX  EML_bestanden_PS2023_deel_2
mv EML_bestanden_PS2023_deel_2 EML_bestanden_PS2023_deel_3
unzip EML_bestanden_PS2023_deel_3.zip
chmod -R u+rwX  EML_bestanden_PS2023_deel_3
``` 

This gets you the results of all provincial elections (Provinciale Staten)
from 2023. Note that these are 12 completely independent elections, one for
each province.

# Running

```bash
./emlconv EML_bestanden_PS2023_deel3/ZuidHolland
sqlite3 eml.sqlite < useful-views
```

This will generate a SQLite database with some useful views, like for
example:

```
-- finds differences between 510d reporting units and the actual reporting unit 510cs
select * from meta,rumeta where meta.formid='510c' and rumeta.formid='510d'
and meta.kieskringHSB = rumeta.kieskringHSB and meta.kind = rumeta.kind  and
meta.value != rumeta.value order by kieskringHSB;

-- finds differences between 510c reporting units and the actual reporting unit 510bs
select * from meta,rumeta where meta.formid='510b' and rumeta.formid='510c'
and meta.gemeente = rumeta.gemeente and meta.kind = rumeta.kind  and
meta.value != rumeta.value order by kieskringHSB;
```

Or how about some statistics (enter this after running 'sqlite3
eml.sqlite'):

```
.mode markdown
.header on

select stembureau, stembureauId, ongeldig, blanco, kiesgerechtigden,
stemmen, volmachten, kiespassen, stempassen, toegelaten 
from sbmeta 
order by volmachten asc;
```

# Tables, philosophy
The idea is that no data is lost in the conversion from EML to SQL. That
means that in theory it should be possible to recreate the EML from the SQL.
This is not currently true however.

Every XML file leads to data in SQL tables. Only very little processing is
done. The lone exception is that roman numeral kieskring numbers are
replaced by normal numbers. This makes it easier to tie tables together.

Here are the tables that are filled out, and the EML form ID that is the
source of that information

 * election (110a): ID of this election, region name, domain code.
 * candentries (230b): List of candidate *entries*. Candidates might appear
   here many times if an election has multiple lists per party, for example
   one per kieskring. Be very careful to always include the kieskringId in
   your queries! Links to affiliations with 'affid'. 
 * affiliations (230b): All affiliations, repeated for each list/kieskring.
   There is a view called 'uniaffili' that has each affiliation just once.
 * candvotecounts (510d, c, b): votes for a candidate entry, as reported in
   the VoteCount part of form ID 510. So these are the counts *at that
   level*. 
 * rucandvotecounts (510d, c, b): votes for a candidate entry, as part of
   the ReportingUnits. So for 510b, these are the actual voting bureaus.
 * affvotecounts/ruaffvotecounts (510d, c, b): the same thing as
   (ru)candvotecounts, except for affiliations
 * candresults (520): candidates that won a seat, and if so, what kind
   (preference votes or not)
 * affresults (520): not overly useful - list of parties that won at least
   one seat
 * meta (510d,c,b): statistics at that level for numbers of ballots 'cast',
   electors admitted etc.
 * rumeta (510d,c,b): same statistics, but for reporting units one level
   lower.
 * stembureaus (510b): generated from 510b, has metadata per stembureau, so
   we don't have to repeat that for every rucandvotes entry.
   stembureaus.id=other.stembureauId

If this election knows about 'shortcodes', these are filled out in
rucandvotecounts and votecounts for formid 510d.

# Views
To make life easy, some views are defined:

 * sbmeta: stembureau metadata pivoted to one row per stembureau
 * uniaffili: one line per affiliation, and its (aff)id
 * .. more to come ..

# CSV generation
The SQLite database can be used to generate CSV output in the format that
verkiezingsuitslagen.nl needs. Use the following SQL scripts for this
purpose:

 * tk-and-ps2script: does Tweede Kamer, multi-kieskring Provinciale Staten
   and Eerste kamer elections
 * ps1script: Single kieskring elections

Then there is a script that generates votingbureau (stembureau) level
output:

 * gsbscript


# Documents, third party links

 * [Electiontool](https://github.com/kroncrv/electiontool), an
   earlier project by Hay Kranen, which does something similar, but then
   converts directly to CSV.
 * [Dutch EML specification](https://www.kiesraad.nl/binaries/kiesraad/documenten/formulieren/2016/osv/eml-bestanden/specificatiedocument-eml_nl-versie-1.0a/specificatiedocument-eml-nl-1.0.a.pdf)

