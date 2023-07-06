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

async function doStuff(f)
{
    return await fetch('elections').then(response => response.json()).then(data => {
        f.electionID=data[0].id;
        f.electionName=data[0].name;
        fetch("gemeentes/"+f.electionID).then(response => response.json()).then(
            data => { f.gemeentes=data;
                      f.gemeentes.sort((a,b) => a.gemeente.localeCompare(b.gemeente));
                      if(f.gemeente == -1)
                          f.gemeente=data[0].gemeenteId;
                      console.log("Set gemeenteId to "+f.gemeente);
                      fetchGemeente(f);
                      fetchGemeenteCands(f);
                      fetchStembureaus(f);
                    }
        );
    });

}

async function fetchGemeente(f)
{
    console.log("Going to fetch for gemeente "+f.gemeente);
    return await fetch('elections').then(response => response.json()).then(data => {
        f.electionID=data[0].id;
        f.electionName=data[0].name;
        fetch("gemeente-affvotecount/"+f.electionID+"/"+f.gemeente).then(response => response.json()).then(data => f.affvotecounts=data);
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
    fetch("stembureau-affvotecount/"+f.electionID+"/"+f.gemeente+"/"+f.stembureau).then(response => response.json()).then(data => f.affvotecounts=data);
}

function fetchStembureauCands(f)
{
    fetch("stembureau-candvotecount/"+f.electionID+"/"+f.gemeente+"/"+f.stembureau+"/"+f.affid)
        .then(response => response.json())
        .then(data => f.candvotecounts=data);
}


