buildDebSbuild(
    defaultTargets: 'wb5 wb6',
    customBuildSteps: {
        stage("Build win32") {
            dir("$PROJECT_SUBDIR") {
                sh 'wbdev root bash -c "apt-get update && apt-get -y install gcc-mingw-w64-i686 && unset CC &&  make win32"'
            }
            sh "mv $PROJECT_SUBDIR/*.exe $RESULT_SUBDIR/"
            archiveArtifacts artifacts: "$RESULT_SUBDIR/*.exe"
        }
    }
)