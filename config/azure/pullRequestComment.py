import github
import azure.devops

print azure.devops.__file__

from azure.devops.connection import Connection
from msrest.authentication import BasicAuthentication

import argparse
import os

credentials = BasicAuthentication( "", os.environ["AZURE_RELEASE_TOKEN"] )
connection = Connection( base_url='https://dev.azure.com/thehaddonyoof', creds=credentials )

core_client = connection.clients.get_core_client()

client = connection.clients_v5_1.get_build_client()

print client

artifact = client.get_artifact( "thehaddonyoof", 44, "testArtifact" )

print artifact
print artifact.resource

# print dir( core_client )

# print core_client.get_projects()[0].name

# print core_client.get_project( "gaffer" )

# parser = argparse.ArgumentParser()

# parser.add_argument( "--organisation", required = True )
# parser.add_argument( "--repo", required = True )
# parser.add_argument( "--pr", required = True, type = int )
# parser.add_argument( "--comment", required = True )

# args = parser.parse_args()

# g = github.Github( os.environ["GITHUB_RELEASE_TOKEN"] )
# repo = g.get_repo( args.organisation + "/" + args.repo )
# #pr = repo.get_pull( args.pr )
# #pr.create_issue_comment( args.comment )


# commit = repo.get_commit( "e5f3b8f" )
# print commit

# commit.create_status( "success", "http://gafferhq.org", "Download available", "Linux Build" )
# commit.create_status( "success", "http://gafferhq.org", "Download available", "MacOS Build" )
