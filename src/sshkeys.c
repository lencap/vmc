// sshkeys.c

#include "vmc.h"

// Create Private/Public SSH keys
void CreateSSHKeys(void)
{
    // Create private SSH key file, and assign path to vmsshpri
    sprintf(vmsshpri, "%s%s%s", vmhome, PATHSEPSTR, sshpri);  // Global vars
    FILE *fp;
    fp = fopen(vmsshpri, "w");
    if (!fp) {
        fprintf(stderr, "Error creating file '%s'\n", vmsshpri);
        Exit(EXIT_FAILURE);
    }
    fprintf(fp, "-----BEGIN RSA PRIVATE KEY-----\n"
        "MIIEoQIBAAKCAQEAyIZo/WEpMT8006pKzqHKhNEAPITJCEWjLN+cGSg9snFXVljA\n"
        "IQ9CtLo89PJvnfGj8I9VxXPxCUmC8gew/XXxQuExa0XhSSNYDEqMyOvlB8KSoYw8\n"
        "tFwNAYaeHw4rbygIgOSn5+g1lLXEf+FPa5JJJAByoxvqXtxZhwiJP2BOkp/ULqsy\n"
        "1UGbHFzGsYHkD8ukYINnr8Yob5K3GuvBSZkb4o02ErC0Tj9Xi52vxgSQEKNQs5BO\n"
        "xzb4gtJ7ozArd11xrpmel02bH7mRfrB/Gpsfvb4WXRG9Kiat09T3XjceMAlcmMUG\n"
        "QJD0ip1mgN3elTCGpon8K5ZRWGxrF7G8XqnGQQIBIwKCAQBKexENp77XxwT+KU78\n"
        "SrjvgNQzvEqrTRC45Vc8i0odtRHPnU6tMY3OGUnXUru+UnAXhbIkxKn8Ip5Z5ZmC\n"
        "tsdTWvUZNzZrn2nYrfnG+IhEtfvy3FEPyms7FL5jTmfndUT8rLNkw/ak8w54pCTQ\n"
        "LwU5Qf6xnKeCUdgcNl7dBoOViyj8eourzUCY55JMZFDF7exKA6j2FXdUsl6O8zlZ\n"
        "immjlbO6dSkZt203jyuFlV1YNG/13EjeihmvMDugnP2CP1qr9kUpGn1NfwdCVb2/\n"
        "AJz+g8gvZflOIzYaEiaodEMJsGNA2sXuygOVSZy4ryc3iFezA4Bvk/3bLweNqlvj\n"
        "yVSLAoGBAP7tjUErm9ciDzLnXe6+toV2E1e1D/a/z2t/GxbbxPYklAUeWaPLhcsZ\n"
        "9B7aUx7BGeB+lPI5sP//hc8ouFTTgE3Y9nsgOjEY7HFIDSG1xrN+MnuY+ztNeDif\n"
        "Jfs44vMdP32pAwJqgtKtg1KiILqx81XeMYhBynpwooISJTioW5FRAoGBAMleSirb\n"
        "GrOZtrsj2s0Uz6LAtPpQfGaQp/5CVYYt/QlUUnR0fhgCaT8DduCrbhOJcmoo5V1K\n"
        "EimEQPhAM6JFMpIuDBp+kQT7FKo1Rl6J+igId5N/jc72nm1kpYBfzGLlAqxU13jJ\n"
        "ULXBU+QU5ZLrRESrTwZun9ytXvwR/O7HMCnxAoGBAPek755k4IfX8YHoEhsfqf39\n"
        "rGPUdehijvq1/Q7kHmtz/YFQrtmhIuKOPZpQbgCeU6bhXX2XIPivFEWVRVm3g/Pa\n"
        "FAKUVclLaVgaG2KTU09HZD2NS9M1UDcBAFMhUX50LwxbCjzcfxXNIHwod5DKH5U+\n"
        "Pr7g01JeycAu4lRLxqprAoGASstAHovlWKbO18t9J5oD+p9ZKcYfk89UVx/z4WGJ\n"
        "3uTOK0E2JizIAXZQudlGJIOCRLAarZ8rUT/AXDUafhmzsqNjlM/suLUHrO83ZPFr\n"
        "izZYTLpZPj5YGgDPwfeyUJ40MWFXWL/NhVZvndvgPeJbL3LUNZbNqby84UiChJMg\n"
        "hJsCgYA5KiUh42OZgHjez5SGvDBoBaOPwutk5O5ufESfgpzh3W0r6iA/dy+HesYV\n"
        "7YDhwSXl7zQgWHKb76SE1/fQcBYOd4TcKzbq4IDU1+oibcOXRMOGkg6vdsXhAvdU\n"
        "ADiALhM7Gqxc0eIMPSw4REhLiS2XNlTSL8fxPHMVJSIuPn+SXw==\n"
        "-----END RSA PRIVATE KEY-----\n");
    fclose(fp);
    int mode = strtol("0600", 0, 8);      // Not entirely clear to me why this conversion is needed
    if ( chmod(vmsshpri, mode) < 0 ) {    // Try chmod 600
        fprintf(stderr, "Error chmod 600 %s\n", vmsshpri);
        Exit(EXIT_FAILURE);
    }

    // Create public SSH key file, from private
    char vmsshpub[256];
    char cmd[256];
    sprintf(vmsshpub, "%s%s%s", vmhome, PATHSEPSTR, sshpub);    // sshpub is global
    sprintf(cmd, "/usr/bin/ssh-keygen -f %s -y > %s", vmsshpri, vmsshpub);
    int rc = system(cmd);
    if (rc == -1) {
        fprintf(stderr, "Error running: %s\n", cmd);
        Exit(EXIT_FAILURE);
    }
}
