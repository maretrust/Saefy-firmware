#ifndef HTML_H
#define HTML_H
const char indexHTML[] = R"=====(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
	<meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=0, maximum-scale=1, minimum-scale=1">

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

        .login-footer {
            margin: 10px 0;
            overlow: hidden;
            float: left;
            width: 100%;
        }

        .input-txt {
            width: 100%;
            padding: 20px 10px;
            background: #5D92BA;
            border: none;
            font-size: 1em;
            color: white;
            border-bottom: 1px dotted rgba(250, 250, 250, 0.4);
        }

            .input-txt:-moz-placeholder {
                color: #323232;
            }

            .input-txt::-moz-placeholder {
                color: #323232;
            }

            .input-txt:-ms-input-placeholder {
                color: #323232;
            }

            .input-txt::-webkit-input-placeholder {
                color: #323232;
            }

            .input-txt:focus {
                background-color: #4478a0;
            }

        .btn {
            padding: 15px 30px;
            border: none;
            background: white;
            color: #5D92BA;
            float: right;
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
                <strong>SAEFY</strong> configuration
            </h1>
			<h5>
				<strong>Device id:</strong> DEVICE_ID
                <br>
				<strong>Firmware ver:</strong> FIRMWARE_VER
            </h5>
            <form action="/configwifi" method="post">
                <br>
                <a class="info">Connect to the following WiFi:</a>
                <input type="text" name="ssid" placeholder="SSID" required="required" class="input-txt" value="Atlantis"/>
                <input type="text" name="password" placeholder="Password" required="required" class="input-txt" value="tiro201."/>
                <br>
                <div class="login-footer">
                    <button type="submit" class="btn">Submit</button>
                </div>
            </form>
        </div>
    </div>
</body>
</html>
)=====";
#endif
