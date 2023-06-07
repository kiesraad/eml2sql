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
mv EML_bestanden_PS2023_deel1 EML_bestanden_PS2023_deel2
unzip EML_bestanden_PS2023_deel_2.zip
mv EML_bestanden_PS2023_deel2 EML_bestanden_PS2023_deel3
unzip EML_bestanden_PS2023_deel_3.zip
```
 

# Running

```bash
./emlconv EML_bestanden_PS2023_deel3
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

# Documents

 * [Dutch EML specification](https://www.kiesraad.nl/binaries/kiesraad/documenten/formulieren/2016/osv/eml-bestanden/specificatiedocument-eml_nl-versie-1.0a/specificatiedocument-eml-nl-1.0.a.pdf)

