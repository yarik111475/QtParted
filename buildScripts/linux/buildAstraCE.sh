#!/bin/bash
set -e

USER_NAME=$(whoami)
CURRENT_DIR=$(pwd)

echo "USER_NAME: ${USER_NAME}"
echo "CURRENT_DIR: ${CURRENT_DIR}"
echo "Create needed directories"

mkdir -p "${CURRENT_DIR}"/build
mkdir -p "${CURRENT_DIR}"/install
mkdir -p "${CURRENT_DIR}"/deploy

HOME_DIR="/home/${USER_NAME}"
BUILD_DIR="${CURRENT_DIR}"/build
INSTALL_DIR="${CURRENT_DIR}"/install
DEPLOY_DIR="${CURRENT_DIR}"/deploy
PARTED_DIR=${BUILD_DIR}/../../../3rdparty/parted

#cmake options
CMAKE_PATH="/usr/bin/cmake"
CMAKE_MAKE_PROGRAM="/usr/bin/ninja"
CMAKE_PREFIX_PATH="${HOME_DIR}/Qt/Qt5.12.12/5.12.12/gcc_64"
QT_QMAKE_EXECUTABLE="${CMAKE_PREFIX_PATH}"/bin/qmake
Qt5_DIR="${CMAKE_PREFIX_PATH}/lib/cmake/Qt5"
CQTDEPLOYER_DIR="${HOME_DIR}/Qt/CQtDeployer/1.5"

#run cmake
cd "${BUILD_DIR}"
${CMAKE_PATH}  ../../.. -DQt5_DIR=$Qt5_DIR`
` -DCMAKE_MAKE_PROGRAM=$CMAKE_MAKE_PROGRAM`
` -DCMAKE_INSTALL_PREFIX=$INSTALL_DIR`
` -DCMAKE_PREFIX_PATH=$CMAKE_PREFIX_PATH`
` -GNinja`
` -DCMAKE_BUILD_TYPE:String=Release`
` -DQT_QMAKE_EXECUTABLE:STRING=$QT_QMAKE_EXECUTABLE`
` -DCMAKE_C_COMPILER:STRING=/usr/bin/gcc`
` -DCMAKE_CXX_COMPILER:STRING=/usr/bin/g++

#build and install
${CMAKE_PATH}  --build . --parallel --target all install

#deploy
${CQTDEPLOYER_DIR}/cqtdeployer.sh -qmake ${QT_QMAKE_EXECUTABLE} -bin ${INSTALL_DIR}/bin/QtParted -targetDir ${DEPLOY_DIR} noTranslations

#copy/move shared libraries
#cp -f ${PARTED_DIR}/lib/libparted.so.2.0.1 ${DEPLOY_DIR}/lib/libparted.so.2.0.1

#remove
rm -rf "${BUILD_DIR}"
rm -rf "${INSTALL_DIR}"
