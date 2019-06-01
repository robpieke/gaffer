import github

import argparse
import os

parser = argparse.ArgumentParser()

parser.add_argument( "--organisation", required = True )
parser.add_argument( "--repo", required = True )
parser.add_argument( "--pr", required = True, type = int )
parser.add_argument( "--comment", required = True )

args = parser.parse_args()

g = github.Github( os.environ["GITHUB_RELEASE_TOKEN"] )
repo = g.get_repo( args.organisation + "/" + args.repo )
pr = repo.get_pull( args.pr )
#pr.create_issue_comment( args.comment )


commit = repo.get_commit( "93df97f" )
print commit

commit.create_status( "success", "http://gafferhq.org", "Download available", "Linux Build" )
commit.create_status( "success", "http://gafferhq.org", "Download available", "MacOS Build" )
