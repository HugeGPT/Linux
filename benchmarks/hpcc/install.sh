#!/usr/bin/env bash
set -Eeuxo pipefail

# config
MPICH_VER="4.0.2"
BLAS_VER="3.10.0"
HPCC_VER="1.5.0"
LAPACK_VER="3.10.1"

# failure message
function __error_handing {
	local last_status_code=$1;
	local error_line_number=$2;
	echo 1>&2 "Error - exited with status $last_status_code at line $error_line_number";
	perl -slne 'if($.+5 >= $ln && $.-4 <= $ln){ $_="$. $_"; s/$ln/">" x length($ln)/eg; s/^\D+.*?$/\e[1;31m$&\e[0m/g;  print}' -- -ln=$error_line_number $0
}

trap '__error_handing $? $LINENO' ERR

# file locator
SOURCE="${BASH_SOURCE[0]:-$0}";
while [ -L "$SOURCE" ]; do # resolve $SOURCE until the file is no longer a symlink
	DIR="$( cd -P "$( dirname -- "$SOURCE"; )" &> /dev/null && pwd 2> /dev/null; )";
	SOURCE="$( readlink -- "$SOURCE"; )";
	[[ $SOURCE != /* ]] && SOURCE="${DIR}/${SOURCE}"; # if $SOURCE was a relative symlink, we need to resolve it relative to the path where the symlink file was located
done
SCRIPT_DIR="$( cd -P "$( dirname -- "$SOURCE"; )" &> /dev/null && pwd 2> /dev/null; )";

# get cpu count
NUMCPUS=`grep -c '^processor' /proc/cpuinfo`

# create root
DEPLOY_DIR="$SCRIPT_DIR/deploy"
if [ ! -d "$DEPLOY_DIR" ]; then
    mkdir "$DEPLOY_DIR"
fi
OUTPUT_DIR="$SCRIPT_DIR/output"
if [ ! -d "$OUTPUT_DIR" ]; then
    mkdir "$OUTPUT_DIR"
fi
RESULT_DIR="$SCRIPT_DIR/results"
if [ ! -d "$RESULT_DIR" ]; then
    mkdir "$RESULT_DIR"
fi
cd "$DEPLOY_DIR"

sudo apt-get -y install libtool-bin gfortran

# download and install mpich
MPICH_URL="https://www.mpich.org/static/downloads/${MPICH_VER}/mpich-${MPICH_VER}.tar.gz"
MPICH_TGZ="mpich-${MPICH_VER}.tar.gz"
MPICH_EXT="mpich-${MPICH_VER}"
MPICH_BLD="$MPICH_EXT/install.venv"
MPICH_DIR="mpich"

if [ ! -e "$MPICH_DIR" ]; then

    cd "$DEPLOY_DIR"
    wget -O "$MPICH_TGZ" "$MPICH_URL"
    tar -xf "$MPICH_TGZ"
    mkdir "$MPICH_BLD"

    cd "$DEPLOY_DIR/$MPICH_EXT"
    ./configure "-prefix=${DEPLOY_DIR}/${MPICH_BLD}"
    time nice make -j$NUMCPUS --load-average=$NUMCPUS
    make install

    cd "$DEPLOY_DIR/$MPICH_BLD/lib"
    ln -sf "libmpi.a" "libmpich.a"

    cd "$DEPLOY_DIR"
    ln -sf "$MPICH_BLD" "$MPICH_DIR"

fi

# download and install blas
BLAS_URL="http://www.netlib.org/blas/blas-${BLAS_VER}.tgz"
BLAS_TGZ="blas-${BLAS_VER}.tar.gz"
BLAS_EXT="BLAS-${BLAS_VER}"
BLAS_BLD="$BLAS_EXT/lib"
BLAS_DIR="blas"

if [ ! -e "$BLAS_DIR" ]; then

    cd "$DEPLOY_DIR"
    wget -O "$BLAS_TGZ" "$BLAS_URL"
    tar -xf "$BLAS_TGZ"

    cd "$DEPLOY_DIR/$BLAS_EXT"
    mkdir "$DEPLOY_DIR/$BLAS_BLD"
    time nice make -j$NUMCPUS --load-average=$NUMCPUS
    mv *.a "$DEPLOY_DIR/$BLAS_BLD"

    cd "$DEPLOY_DIR/$BLAS_BLD"
    ln -sf "blas_LINUX.a" "libblas.a"

    cd "$DEPLOY_DIR"
    ln -sf "$BLAS_BLD" "$BLAS_DIR"

fi

# download and install cblas
CBLAS_URL="http://www.netlib.org/blas/blast-forum/cblas.tgz"
CBLAS_TGZ="cblas.tgz"
CBLAS_EXT="CBLAS"
CBLAS_BLD="$CBLAS_EXT/lib"
CBLAS_DIR="cblas"

if [ ! -e "$CBLAS_DIR" ]; then

    cd "$DEPLOY_DIR"
    wget -O "$CBLAS_TGZ" "$CBLAS_URL"
    tar -xf "$CBLAS_TGZ"

    cd "$DEPLOY_DIR/$CBLAS_EXT"
    ln -sf "Makefile.LINUX" "Makefile.in"
    perl -p -i -e 's/^BLLIB\b.*/BLLIB = ..\/..\/blas\/libblas.a/g' Makefile.LINUX
    time nice make alllib -j$NUMCPUS --load-average=$NUMCPUS
    time nice make all -j$NUMCPUS --load-average=$NUMCPUS

    cd "$DEPLOY_DIR/$CBLAS_BLD"
    ln -sf "cblas_LINUX.a" "libcblas.a"

    cd "$DEPLOY_DIR"
    ln -sf "$CBLAS_BLD" "$CBLAS_DIR"

fi

# download and install lapack
LAPACK_URL="https://github.com/Reference-LAPACK/lapack/archive/refs/tags/v${LAPACK_VER}.tar.gz"
LAPACK_TGZ="lapack-${LAPACK_VER}.tar.gz"
LAPACK_EXT="lapack-${LAPACK_VER}"
LAPACK_BLD="$LAPACK_EXT/lib"
LAPACK_DIR="lapack"

if [ ! -e "$LAPACK_DIR" ]; then

    cd "$DEPLOY_DIR"
    wget -O "$LAPACK_TGZ" "$LAPACK_URL"
    tar -xf "$LAPACK_TGZ"

    cd "$DEPLOY_DIR/$LAPACK_EXT"
    mkdir "$DEPLOY_DIR/$LAPACK_BLD"
    cp "make.inc.example" "make.inc"
    time nice make -j$NUMCPUS --load-average=$NUMCPUS

    cd "$DEPLOY_DIR/$LAPACK_EXT/LAPACKE"
    time nice make -j$NUMCPUS --load-average=$NUMCPUS

    cd "$DEPLOY_DIR/$LAPACK_EXT"
    mv *.a "$DEPLOY_DIR/$LAPACK_BLD"

    cd "$DEPLOY_DIR"
    ln -sf "$LAPACK_BLD" "$LAPACK_DIR"

fi

sudo apt-get -y install libblas-dev liblapack-dev liblapacke-dev

# download and install hpcc
HPCC_URL="http://icl.cs.utk.edu/projectsfiles/hpcc/download/hpcc-${HPCC_VER}.tar.gz"
HPCC_TGZ="hpcc-${HPCC_VER}.tar.gz"
HPCC_EXT="hpcc-${HPCC_VER}"
HPCC_BLD="$HPCC_EXT"
HPCC_DIR="hpcc"

if [ ! -e "$HPCC_DIR" ]; then

    cd "$DEPLOY_DIR"
    wget -O "$HPCC_TGZ" "$HPCC_URL"
    tar -xf "$HPCC_TGZ"

    cd "$DEPLOY_DIR/$HPCC_EXT/hpl"
    ln -sf "$SCRIPT_DIR/Make.HPCC_LINUX" "Make.LINUX"
    
    cd "$DEPLOY_DIR/$HPCC_EXT"
    make arch=LINUX
    cp "_hpccinf.txt" "hpccinf.txt"

    ln -sf "../../hpcc_impl.sh" "run.sh"
    sudo chmod +x run.sh

    cd "$DEPLOY_DIR"
    ln -sf "$HPCC_BLD" "$HPCC_DIR"

fi

echo "Install done"
