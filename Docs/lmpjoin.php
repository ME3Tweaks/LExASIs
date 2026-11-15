<?php
// Bounces https://me3tweaks.com/modmanager/protocol/lmpjoin?code=...&server=... to
// me3tweaksmodmanager://lmpjoin?code=...&server=... . MilanHost copies these links when the host presses
// Invite Friends; unlike the protocol link, chat apps make them clickable.
// The invite code is a secret, so it is never sent on to other sites or indexed.
header('Referrer-Policy: no-referrer');
header('X-Robots-Tag: noindex');
?>
<html>
    <head>
        <meta name="robots" content="noindex">
        <title>Join LMP lobby</title>
        <script>
            // Links leave the server out when it is this one.
            const defaultServer = 'https://vps.me3tweaks.com';

            // Mod Manager and MilanHost enforce the same limits. server is null for the default.
            function readJoin() {
                let searchParams = new URLSearchParams(window.location.search);
                let code = searchParams.get('code');
                let server = searchParams.get('server');
                if (code === null || !/^[\x21-\x7E]{1,64}$/.test(code)) return null;
                if (server !== null) {
                    if (server.length > 255) return null;
                    try {
                        let parsed = new URL(server);
                        if (parsed.protocol !== 'http:' && parsed.protocol !== 'https:') return null;
                    } catch {
                        return null;
                    }
                }
                return { code: code, server: server };
            }

            window.onload = function(){
                let join = readJoin();
                if (join === null) {
                    document.getElementById('loading').style.display = 'none';
                    document.getElementById('status').textContent = 'This join link is not valid. Ask for a new one.';
                    document.getElementById('details').style.display = 'none';
                    return;
                }

                let protocolParams = new URLSearchParams();
                protocolParams.append('code', join.code);
                if (join.server !== null) protocolParams.append('server', join.server);
                let protocolLink = `me3tweaksmodmanager://lmpjoin?${protocolParams.toString()}`;

                // Shown as text, never as markup: both come from the link.
                document.getElementById('code').textContent = join.code;
                document.getElementById('server').textContent = join.server ?? defaultServer;
                document.getElementById('open').href = protocolLink;

                var iframe = document.createElement("iframe");
                iframe.src = protocolLink;
                iframe.name = "frame"
                iframe.style = "display:none;";
                document.body.appendChild(iframe);

                // Browsers only let a page close a tab that has no history of its own, such as one a chat app
                // opened. If this one stays open, say so instead.
                setTimeout(function() {
                    window.close();
                    document.getElementById('loading').style.display = 'none';
                    document.getElementById('status').textContent = 'ME3Tweaks Mod Manager should be loading. You can close this tab.';
                }, 10000);
            }
        </script>
        <style>
            body {
                color: rgb(195,196,197);
                font-family: 'Courier New';
                background-color: rgb(1,41,64);
            }

            a {
                color: rgb(120,190,240);
            }

            #launchtext {
                height: 100%;
                display: flex;
                flex-direction: column;
                justify-content: center;
                align-items: center;
            }

            p {
                max-width: 600px;
                text-align: center;
                overflow-wrap: anywhere;
            }

            /* Three dots hopping in turn while Mod Manager starts. */
            #loading {
                display: flex;
                gap: 12px;
                height: 40px;
                align-items: flex-end;
                margin-top: 24px;
            }

            #loading span {
                width: 14px;
                height: 14px;
                border-radius: 50%;
                background-color: rgb(120,190,240);
                animation: hop 1.2s ease-in-out infinite;
            }

            #loading span:nth-child(2) {
                animation-delay: 0.15s;
            }

            #loading span:nth-child(3) {
                animation-delay: 0.3s;
            }

            @keyframes hop {
                0%, 60%, 100% {
                    transform: translateY(0) scale(1);
                    opacity: 0.4;
                }
                30% {
                    transform: translateY(-22px) scale(1.15);
                    opacity: 1;
                }
            }

            /* No hopping for people who turned motion down; a slow fade still shows it is working. */
            @media (prefers-reduced-motion: reduce) {
                #loading span {
                    animation: fade 1.8s ease-in-out infinite;
                }

                @keyframes fade {
                    0%, 100% { opacity: 0.3; }
                    50% { opacity: 1; }
                }
            }
        </style>
    </head>
    <body>
        <div id="launchtext">
            <div id="loading" aria-hidden="true"><span></span><span></span><span></span></div>
            <h3 id="status" role="status">ME3Tweaks Mod Manager should be loading. This tab closes in 10 seconds.</h3>
            <div id="details">
                <p>Make sure you let your browser open me3tweaksmodmanager:// links with ME3Tweaks Mod Manager or this won't work. <a id="open" href="#">Open Mod Manager</a></p>
                <p>Invite code: <b id="code"></b><br>Matchmaking server: <span id="server"></span></p>
            </div>
        </div>
    </body>
</html>
