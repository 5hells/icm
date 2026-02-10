const { IcmShell, createPixelEffect } = require('./icm-shell');

const socketPath = process.env['XDG_RUNTIME_DIR'] + '/icm.sock';
console.log('Connecting to socket:', socketPath);

const shell = new IcmShell(socketPath);

shell.on('connect', () => {
    console.log('Connected to ICM compositor');

    // Test advanced screen effects
    console.log('Testing advanced screen effects...');

    // Create a blur effect using the advanced syntax
    const blurEffect = createPixelEffect()
        .blur(3.0)  // Blur with radius 3.0
        .build();

    console.log('Applying blur effect:', blurEffect);
    shell.setScreenEffect(blurEffect, true);

    // Wait a bit, then apply another effect
    setTimeout(() => {
        const brightnessEffect = createPixelEffect()
            .brightness(0.7)
            .build();

        console.log('Applying brightness effect:', brightnessEffect);
        shell.setScreenEffect(brightnessEffect, true);

        // Wait, then disable
        setTimeout(() => {
            console.log('Disabling screen effect');
            shell.setScreenEffect('', false);
        }, 3000);
    }, 3000);
});