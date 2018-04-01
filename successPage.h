#ifndef SUCCESSPAGE_H
#define SUCCESSPAGE_H
const char successHTML[] = R"=====(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
	<meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=0, maximum-scale=1, minimum-scale=1">
	<meta http-equiv="refresh" content="10">

    <style>
        html {
            background-color: #5D92BA;
            font-family: sans-serif;
        }

        .container {
            width: 100%;
            height: 400px;
            margin: 0 auto;
        }

        .login {
            margin-top: 10px;
            width: 90%;
        }

        .login-heading {
            font: 1.8em/48px sans-serif;
            color: white;
        }

        .info {
            font-size: .8em;
            line-height: 3em;
            color: white;
            text-decoration: none;
        }
    </style>

    <title>SAEFY access point</title>
</head>
<body>
    <div class="container">
        <div class="login">
            <h1 class="login-heading">
                <strong>SAEFY</strong> configuration access point
            </h1>
            <form method="get">
                <br>
                <a class="info">Testing connections. Please, check leds on Saefy:</a>
                <br>
                <a class="info">Green led on: connections were successful; settings will be saved</a>
                <br>
                <a class="info">Red led on: connections were unsuccessful; settings will be discarded</a>
            </form>
        </div>
    </div>
</body>
</html>
)=====";
#endif
