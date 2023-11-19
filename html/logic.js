var existingKieskringen={};

function doFetch(f)
{
    fetch('elections').then(response => response.json()).then(data => {
        console.log(data[0].id);
        f.electionID=data[0].id;
        f.electionName=data[0].name;
    }).then(
        function() {
            let f1=fetch('kieskringen/'+f.electionID)
                .then(response => response.json())
                .then(data => {f.kieskringen=data; f.message = data[0];});
            
            var f2=fetch('affiliations/'+f.electionID).then(response => response.json())
                .then(data => {
                    for (const aff of data)
                    {
                        if(f.grid[aff.id] == undefined) {
                            f.grid[aff.id]={}
                        }
                        f.grid[aff.id]["name"]=aff.name;
                        f.grid[aff.id]["id"]=aff.id;
                        if(f.grid[aff.id]["kiesk"] ==undefined)
                        {
                            f.grid[aff.id]["kiesk"]={};
                        };
                        
                        f.grid[aff.id].kiesk[aff.kieskringId]="⊙";
                        existingKieskringen[aff.kieskringId]=1;
                    }
                    // zero out all missing kieskringen
                    for(aff in f.grid) {
                        for(kiesk in existingKieskringen) {
                            if(f.grid[aff].kiesk[kiesk] == undefined)
                                f.grid[aff].kiesk[kiesk]='';
                        }
                    }
                    console.log(f.grid);
                } );
            
            var f3=fetch('totaaltellingen/'+f.electionID).then(response=>response.json()).then(data => {
                for(const affvotes of data) {
                    if(f.grid[affvotes.affid] == undefined) {
                        f.grid[affvotes.affid]={}
                    }
                    f.totaffs510d += parseInt(affvotes.votes510d);
                    f.totaffs510c += parseInt(affvotes.votes510c);
                    f.totaffs510b += parseInt(affvotes.votes510b);
                    f.totaffs510a += parseInt(affvotes.votes510a);
                    
                    
                    f.grid[affvotes.affid].totvotes = affvotes.votes510d;
                    f.grid[affvotes.affid].votes510c = affvotes.votes510c;
                    f.grid[affvotes.affid].votes510b = affvotes.votes510b;
                    f.grid[affvotes.affid].votes510a = affvotes.votes510a;
                    f.grid[affvotes.affid].discrepancy = (affvotes.discrepancy=="true");
                }
            });
            var f4 = fetch('totaaltellingen-per-kieskring/'+f.electionID).then(response=>response.json()).then(data => {
                for(const votes of data) {
                    if(f.kieskringVotes[votes.kieskringId] == undefined) {
                        f.kieskringVotes[votes.kieskringId]={}
                    }
                    f.totkieskringen510d += parseInt(votes.votes510d);
                    f.totkieskringen510c += parseInt(votes.votes510c);
                    f.totkieskringen510b += parseInt(votes.votes510b);
                    f.totkieskringen510a += parseInt(votes.votes510a);
                    f.kieskringVotes[votes.kieskringId].totvotes = votes.votes510d;
                    f.kieskringVotes[votes.kieskringId].votes510c = votes.votes510c;
                    f.kieskringVotes[votes.kieskringId].votes510b = votes.votes510b;
                    f.kieskringVotes[votes.kieskringId].votes510a = votes.votes510a;
                    f.kieskringVotes[votes.kieskringId].discrepancy = (votes.discrepancy=="true");
                }
            });
            Promise.all([f1,f2,f3,f4]).then((values) => {
                console.log(values);
            });
        });
    
}

function fetchCandidates(f)
{
    fetch('elections').then(response => response.json()).then(data => {
        console.log(data[0].id);
        f.electionID=data[0].id;
        f.electionName=data[0].name;
        fetch("candentries/"+f.electionID+"/4/1").then(response => response.json()).then(data => f.candidates=data)
    });
}

function getGemeentesMeta(f)
{
    fetch('elections').then(response => response.json()).then(data => {
        f.electionID=data[0].id;
        f.electionName=data[0].name;

        fetch("gemeentes-meta/"+f.electionID).then(response => response.json()).then(
            data => { f.gemeentes=data;
                      f.gemeentes.sort((a,b) => a.gemeente.localeCompare(b.gemeente));
                    }
        );
    });
}

async function doStuff(f)
{
    return await fetch('elections').then(response => response.json()).then(data => {
        f.electionID=data[0].id;
        f.electionName=data[0].name;

        fetch('kieskringen/'+f.electionID)
            .then(response => response.json())
            .then(data => {f.kieskringen=data; console.log(data);});
       
        a = new URL(window.location.href)
        getGemeentesMeta(f);
        if(a.searchParams.has("gemeente") && a.searchParams.has("stembureau")) {
            f.gemeente=a.searchParams.get("gemeente");
            f.stembureau=a.searchParams.get("stembureau");
            fetchStembureau(f); fetchStembureauCands(f); fetchStembureauMeta(f);
        }
        else {
            fetch("totaaltelling-aff/"+f.electionID).then(r => r.json()).then(d =>
                {
                    f.affvotecounts = d;
                    var totalvotes=0;
                    for (const aff of d) {
                        totalvotes += parseInt(aff.votes);
                    }
                    f.totalvotes = totalvotes;
                    console.log(`Total votes ${totalvotes}`);
                })
            fetch("totaaltelling-affcand/"+f.electionID+"/"+f.affid).then(r => r.json()).then(d => { f.candvotecounts = d; console.log(d);})
            fetchMeta(f);
        }
    });

}

async function fetchGemeente(f)
{
    console.log("Going to fetch for gemeente "+f.gemeente);
    return await fetch('elections').then(response => response.json()).then(data => {
        f.electionID=data[0].id;
        f.electionName=data[0].name;
        fetch("gemeente-affvotecount/"+f.electionID+"/"+f.gemeente).then(response => response.json()).then(data => {
            f.affvotecounts=data
            f.totalvotes=0
            for (const aff of data) {
                f.totalvotes += parseInt(aff.votes);
            }
        });
    });
}

async function fetchGemeenteCands(f)
{
    console.log("Going to fetch for gemeente "+f.gemeente+" and affid "+f.affid);
    fetch("gemeente-candaffvotecount/"+f.electionID+"/"+f.gemeente+"/"+f.affid).then(response => response.json()).then(data => f.candvotecounts=data);
}

function fetchStembureaus(f)
{
    fetch("stembureaus/"+f.electionID+"/"+f.gemeente).then(response => response.json()).then(data => f.stembureaus=data);
}

function fetchStembureau(f)
{
    fetch("stembureau-affvotecount/"+f.electionID+"/"+f.gemeente+"/"+f.stembureau).then(response => response.json()).then(data =>
        {
            f.affvotecounts=data;
            f.totalvotes=0
            for (const aff of data) {
                f.totalvotes += parseInt(aff.votes);
            }
            
        });
}

function fetchStembureauCands(f)
{
    fetch("stembureau-candvotecount/"+f.electionID+"/"+f.gemeente+"/"+f.stembureau+"/"+f.affid)
        .then(response => response.json())
        .then(data => f.candvotecounts=data);
}

function fetchStembureauMeta(f)
{
    fetch("rawsb-meta/"+f.electionID+"/"+f.gemeente+"/"+f.stembureau)
        .then(response => response.json())
        .then(data => {console.log(data); f.rawmeta=data; });
}

function fetchGemeenteStembureauMeta(f)
{
    a = new URL(window.location.href)
    f.gemeente=a.searchParams.get("gemeente")

    fetch('elections').then(response => response.json()).then(data => {
        f.electionID=data[0].id;
        f.electionName=data[0].name;
        
        fetch("gemeente-sbmeta/"+f.electionID+"/"+f.gemeente)
            .then(response => response.json())
            .then(data => {console.log(data); f.stembureaus=data; f.gemeenteNaam = data[0].gemeente});
    });
}


function fetchGemeenteMeta(f)
{
    fetch("rawgemeente-meta/"+f.electionID+"/"+f.gemeente)
        .then(response => response.json())
        .then(data => {console.log(data); f.rawmeta=data; });
}

function fetchMeta(f)
{
    fetch("raw-meta/"+f.electionID)
        .then(response => response.json())
        .then(data => {console.log(data); f.rawmeta=data; });
}



