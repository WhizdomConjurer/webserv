#! /usr/bin/python3
from http import cookies
import os
import cgi
import time
import hashlib
import pickle
import sys

class Session:
    """Create and persist one login session for a user."""
    def __init__(self, name):
        self.name = name
        self.sid = hashlib.sha1(str(time.time()).encode("utf-8")).hexdigest()
        os.makedirs('cgi-bin/sessions', exist_ok=True)
        with open('cgi-bin/sessions/session_' + self.sid, 'wb') as f:
            pickle.dump(self, f)

    """Return the session identifier used as cookie value."""
    def getSid(self):
        return self.sid

class UserDataBase:
    """Store registered users and their display data in a pickle file."""
    def __init__(self):
        self.user_pass = {}
        self.user_firstname = {}

    """Add or update one user and persist the database."""
    def addUser(self, username, password, firstname):
        self.user_pass[username] = password
        self.user_firstname[username] = firstname
        with open('cgi-bin/user_database', 'wb') as f:
            pickle.dump(self, f)


def printAccPage(session):
    """Print the account page for an authenticated session."""
    print("Content-type: text/html\r\n")
    print("<html>")
    print("<head>")
    print("<title>Account Page</title>")
    print("</head>")
    print("<body>")
    print("<h1>Welcome Again", session.name, "!</h1>")
    print("<p>Your Session ID is: ", session.getSid(), "</p>")
    print("</body>")
    print("<a href=\"/index.html\"> Click here to go back to homepage </a>")
    print("</html>")

def printUserMsg(msg):
    """Print a small HTML page with one user-facing message."""
    print("Content-type: text/html\r\n")
    print("<html>")
    print("<head>")
    print("<title>USER MSG</title>")
    print("</head>")
    print("<body>")
    print("<h1>", msg ,"</h1>")
    print("</body>")
    print("<a href=\"/login.html\"> Click here to go back to login page </a>")
    print("</html>")

def printLogin():
    """Print the login form when no usable credentials were submitted."""
    print("Content-Type: text/html\r\n")
    print("<html> ")
    print("<head>")
    print("<meta charset=\"UTF-8\" name=\"viewport\" content=\"width=device-width, initial-scale=1\">")
    print("<link rel=\"stylesheet\" href=\"/assets/css/accstyle.css\">")
    print("<title> Login Page </title>")
    print("</head>")
    print("<body>  ")
    print("<center> <h1> Amanix Login Form </h1> </center> ")
    print("<form action = \"../cgi-bin/acc.py\" method = \"get\">")
    print("<div class=\"container\"> ")
    print("<label>Username : </label> ")
    print("<input type=\"text\" placeholder=\"Enter Username\" name=\"username\" required>")
    print("<label>Password : </label> ")
    print("<input type=\"password\" placeholder=\"Enter Password\" name=\"password\" required>")
    print("<button type=\"submit\">Login</button> ")
    print("No Account?<a href=\"/register.html\"> Register Here </a> ")
    print("</div> ")
    print("</form>   ")
    print("</body>   ")
    print("</html>")




def authUser(name, password):
    """Return a new session if the supplied credentials match the database."""
    if os.path.exists('cgi-bin/user_database'):
        with open('cgi-bin/user_database', 'rb') as f:
            database = pickle.load(f)
            if name in database.user_pass and database.user_pass[name] == password:
                session = Session(database.user_firstname[name])
                return session
            else:
                return None
    else:
        return None

def handleLogin():
    """Handle login, registration, or default form display based on form fields."""
    username = form.getvalue('username')
    password = form.getvalue('password')
    firstname = form.getvalue('firstname')
    if username is None:
        printLogin()
    elif firstname is None:
        session = authUser(form.getvalue('username'), form.getvalue('password'))
        if session is None:
            printUserMsg("Failed to login, username or password is wrong!")
        else:
            print("Correct credentials", file=sys.stderr)
            response_cookie = cookies.SimpleCookie()
            response_cookie["SID"] = session.getSid()
            response_cookie["SID"]["max-age"] = 120
            print("Status: 302 Found")
            print(response_cookie.output())
            print("Location: acc.py")
            print("\r\n")
    else :
        if os.path.exists('cgi-bin/user_database'):
            with open('cgi-bin/user_database', 'rb') as f:
                database = pickle.load(f)
                if username in database.user_pass:
                    printUserMsg("Username is already registered!")
                else:
                    database.addUser(username, password, firstname)
                    printUserMsg("Account registered successfully!")
        else:
            database = UserDataBase()
            if username in database.user_pass:
                printUserMsg("Username is already registered!")
            else:
                database.addUser(username, password, firstname)
                printUserMsg("Account registered successfully!")

form = cgi.FieldStorage()
cookies = cookies.SimpleCookie()
if 'HTTP_COOKIE' in os.environ: 
    cookies.load(os.environ["HTTP_COOKIE"])

    if "SID" in cookies:
        print("Your Session ID is", cookies["SID"].value,file=sys.stderr)
        session_path = 'cgi-bin/sessions/session_' + cookies["SID"].value
        if os.path.exists(session_path):
            with open(session_path, 'rb') as f:
                sess = pickle.load(f)
            printAccPage(sess)
        else:
            handleLogin()
    else:
        handleLogin()    
else:
    handleLogin()
