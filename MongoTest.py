from pymongo import MongoClient
from pymongo.errors import ConnectionFailure

uri = "mongodb+srv://asanders_db_user:RJkvLEBAKiwSF1xt@cluster0.2img3k7.mongodb.net/MarbleMetrics?retryWrites=true&w=majority"

try:
    client = MongoClient(uri, serverSelectionTimeoutMS=5000)
    # The ismaster command is cheap and does not require auth.
    client.admin.command('ismaster')
    print("MongoDB connection successful!")
    print("Databases:", client.list_database_names())
except ConnectionFailure as e:
    print("MongoDB connection failed:", e)