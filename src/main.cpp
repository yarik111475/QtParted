#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QJsonArray>
#include <QString>
#include <QUuid>
#include <QDebug>
#include <QFile>
#include <QMap>
#include <QSet>
#include <iostream>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/hdreg.h>

#include <ext2fs/ext2fs.h>
#include <ext2fs/ext2_fs.h>
#include <ext2fs/ext2_types.h>

#include <blkid/blkid.h>
#include <sys/statvfs.h>
#include <parted/parted.h>
#include "ext2_fs.h"
#include <time.h>

#define EXT2_MIN_BLOCK_LOG_SIZE		10

//from e2fsprogs->misc->dunpe2fs.c->e2fsprogs->lib->e2p->ls.c
QMap<QString,QVariant> getFilesysObject(const QString& partPath)
{
    int flags {EXT2_FLAG_JOURNAL_DEV_OK | EXT2_FLAG_SOFTSUPP_FEATURES | EXT2_FLAG_64BITS};
    ext2_filsys fs;
    errcode_t retVal {ext2fs_open(qPrintable(partPath),flags,0,0,unix_io_manager,&fs)};
    if(!retVal){
        auto super {fs->super};
        if(super){
            const auto volumeName {super->s_volume_name[0] ? QString(super->s_volume_name).simplified()
                                                              : QString {}};
            const auto lastMountedOn {super->s_last_mounted[0] ? QString(super->s_last_mounted).simplified()
                                                                  : QString {}};
            const auto bUuid {QByteArray::fromRawData((const char*)super->s_uuid,sizeof(super->s_uuid))};
            const auto qUuid {QUuid::fromRfc4122(bUuid)};
            const auto filesystemUuid {qUuid.toString(QUuid::WithoutBraces)};
            const auto filesystemMagicNumber {super->s_magic};
            const auto filesystemRevision {super->s_rev_level};

            QStringList fileSystemFlags {};
            if(super->s_flags & EXT2_FLAGS_SIGNED_HASH){
                fileSystemFlags.push_back("signed_directory_hash");;
            }
            if(super->s_flags & EXT2_FLAGS_UNSIGNED_HASH){
                fileSystemFlags.push_back("EXT2_FLAGS_UNSIGNED_HASH");
            }
            if(super->s_flags & EXT2_FLAGS_TEST_FILESYS){
                fileSystemFlags.push_back("test_filesystem");
            }

            const auto mountOptions {super->s_mount_opts[0] ? QString((char*)super->s_mount_opts).simplified()
                                                            : QString{}};
            auto fileSystemState {QStringList{}};
            if(super->s_state & EXT2_VALID_FS){
                fileSystemState.push_back("clean");
            }
            else{
                fileSystemState.push_back("not_clean");
            }
            if(super->s_state & EXT2_ERROR_FS){
                fileSystemState.push_back("with_errors");
            }

            auto fileSystemErrors {QString{"Unknown (continue)"}};
            if(super->s_errors==EXT2_ERRORS_CONTINUE){
                fileSystemErrors="Continue";
            }
            else if(super->s_errors==EXT2_ERRORS_RO){
                fileSystemErrors="Remount read-only";
            }
            else if(super->s_errors==EXT2_ERRORS_PANIC){
                fileSystemErrors="Panic";
            }

            const auto getFilesystemOsType{[](int type){
                    const std::map<int,QString> typesMap {
                        {0,"Linux"},
                        {1,"Hurd"},
                        {2,"Masix"},
                        {3,"FreeBSD"},
                        {4,"Lites"}
                    };
                    if(type >= 0 && type <= EXT2_OS_LITES){
                        auto it {typesMap.find(type)};
                        if(it!=typesMap.end()){
                            return it->second;
                        }
                    }
                    return QString {"(unknown os)"};
                }
            };
            const auto filesystemOsType {getFilesystemOsType(super->s_creator_os)};
            const long long inodeCount {static_cast<long long>(super->s_inodes_count)};
            const long long blockCount {static_cast<long long>(super->s_blocks_count) |
                                           ext2fs_has_feature_64bit(super) ? static_cast<long long>(super->s_blocks_count_hi << 32) : 0
                                       };
            const long long reservedBlockCount {static_cast<long long>(super->s_r_blocks_count) |
                                                   ext2fs_has_feature_64bit(super) ? static_cast<long long>(super->s_r_blocks_count_hi << 32) : 0
                                               };
            const long long overheadClusters {static_cast<long long>(super->s_overhead_blocks)};
            const long long freeBlocks {static_cast<long long>(super->s_free_blocks_count)};
            const long long freeInodes {static_cast<long long>(super->s_free_inodes_count)};
            const long long firstBlock {static_cast<long long>(super->s_first_data_block)};
            const long long blockSize {EXT2_BLOCK_SIZE(super)};
            const long long inodesPerGroup {super->s_inodes_per_group};
            const long long inodeBlocksPerGroup {(((super->s_inodes_per_group *
                                                        EXT2_INODE_SIZE(super)) +
                                                        EXT2_BLOCK_SIZE(super) - 1) /
                                                        EXT2_BLOCK_SIZE(super))};
            const long long mountCount {super->s_mnt_count};
            const long long maxMountCount {super->s_max_mnt_count};
            const long long checkedInterval {super->s_checkinterval};

            QMap<QString,QVariant> filesysObject{
                {"volume_name",volumeName},
                {"last_mounted_on",lastMountedOn},
                {"filesystem_uuid",filesystemUuid},
                {"filesystem_magic_number",filesystemMagicNumber},
                {"filesystem_revision",(int)filesystemRevision},
                {"filesystem_flags",fileSystemFlags.join(",")},
                {"mount_options",mountOptions},
                {"filesystem_state",fileSystemState.join(",")},
                {"filesystem_errors",fileSystemErrors},
                {"filesystem_os_type",filesystemOsType},
                {"inode_count",inodeCount},
                {"block_count",blockCount},
                {"reserved_block_count",reservedBlockCount},
                {"overhead_clusters",overheadClusters},
                {"free_blocks",freeBlocks},
                {"free_inodes",freeInodes},
                {"first_block",firstBlock},
                {"block_size",blockSize},
                {"inodes_per_group",inodesPerGroup},
                {"inode_blocks_per_group",inodeBlocksPerGroup},
                {"mount_count",mountCount},
                {"max_mount_count",maxMountCount},
                {"checked_interval",checkedInterval}
            };
            if(ext2fs_has_feature_bigalloc(super)){
                filesysObject.insert("cluster_size",EXT2_CLUSTER_SIZE(super));
            }
            else{
                filesysObject.insert("fragment_size",EXT2_CLUSTER_SIZE(super));
            }
            if(ext2fs_has_feature_64bit(super)){
                filesysObject.insert("group_descriptor_size",static_cast<long long>(super->s_desc_size));
            }
            if(super->s_reserved_gdt_blocks){
                filesysObject.insert("reserved_GDT_blocks",static_cast<uint16_t>(super->s_reserved_gdt_blocks));
            }
            filesysObject.insert("blocks_per_group",static_cast<long long>(super->s_blocks_per_group));
            if(ext2fs_has_feature_bigalloc(super)){
                filesysObject.insert("cluster_per_group",static_cast<long long>(super->s_clusters_per_group));
            }
            else{
                filesysObject.insert("fragment_per_group",static_cast<long long>(super->s_clusters_per_group));
            }
            if(super->s_raid_stride){
                filesysObject.insert("raid_stride",static_cast<int>(super->s_raid_stride));
            }
            if(super->s_first_meta_bg){
                filesysObject.insert("first_meta_block_group",static_cast<long long>(super->s_first_meta_bg));
            }
            if(super->s_log_groups_per_flex){
                filesysObject.insert("flex_block_group_size",static_cast<long long>(1U <<super->s_log_groups_per_flex));
            }
            if(super->s_mkfs_time){
                time_t tm=super->s_mkfs_time;
                filesysObject.insert("filesystem_created",QString(ctime(&tm)).simplified());
            }
            time_t mtime {super->s_mtime};
            filesysObject.insert("last_mount_time",super->s_mtime ? QString(ctime(&mtime)).simplified() : QString{});

            time_t wtime {super->s_wtime};
            filesysObject.insert("last_write_time",QString(ctime(&wtime)).simplified());

            time_t lastcheck {super->s_lastcheck};
            filesysObject.insert("last_checked",QString(ctime(&lastcheck)).simplified());

            if(super->s_checkinterval){
                time_t next {super->s_lastcheck + super->s_checkinterval};
                filesysObject.insert("next_check_after",QString(ctime(&next)).simplified());
            }
            filesysObject.insert("reserved_blocks_uid",super->s_def_resuid);
            filesysObject.insert("reserved_blocks_guid",super->s_def_resgid);

            if(super->s_rev_level >= EXT2_DYNAMIC_REV){
                filesysObject.insert("first_inode",static_cast<long long>(super->s_first_ino));
                filesysObject.insert("inode_size",static_cast<long long>(super->s_inode_size));
                if(super->s_min_extra_isize)
                    filesysObject.insert("required_extra_isize",static_cast<int>(super->s_min_extra_isize));
                if (super->s_want_extra_isize)
                    filesysObject.insert("desired_extra_isize",static_cast<int>(super->s_want_extra_isize));
            }
            if(super->s_journal_uuid[0]){
                const auto bUuid {QByteArray::fromRawData((const char*)super->s_journal_uuid,sizeof(super->s_journal_uuid))};
                const auto qUuid {QUuid::fromRfc4122(bUuid)};
                filesysObject.insert("journalUuid",qUuid.toString(QUuid::WithoutBraces));
            }
            if(super->s_journal_inum)
                filesysObject.insert("journal_inode",static_cast<long long>(super->s_journal_inum));
            if(super->s_journal_dev)
                filesysObject.insert("journal_device",static_cast<long long>(super->s_journal_dev));
            if(super->s_last_orphan)
                filesysObject.insert("first_orphan_inode",static_cast<long long>(super->s_last_orphan));

            if(ext2fs_has_feature_dir_index(super) || super->s_def_hash_version){
                const auto getHash{[](char version){
                        const std::map<char,QString> hashesMap {
                            {EXT2_HASH_LEGACY,"legacy"},
                            {EXT2_HASH_HALF_MD4,"half_md4"},
                            {EXT2_HASH_TEA,"tea"}
                        };
                        const auto it {hashesMap.find(version)};
                        if(it!=hashesMap.cend()){
                            return it->second;
                        }
                        return QString{};
                    }
                };
                filesysObject .insert("default_directory_hash",getHash(super->s_def_hash_version));
            }

            if(sizeof(super->s_hash_seed)!=0){
                const auto bUuid {QByteArray::fromRawData((const char*)super->s_hash_seed,sizeof(super->s_hash_seed))};
                const auto qUuid {QUuid::fromRfc4122(bUuid)};
                filesysObject.insert("directory_hash_seed",qUuid.toString(QUuid::WithoutBraces));
            }
            return filesysObject;
        }
    }
    return QMap<QString,QVariant> {};
}

QJsonArray getMountpointObjects()
{
    QJsonArray mountpointObjects {};
    {//mount_ponts
        const QString fstabPath {"/etc/fstab"};
        QFile file {fstabPath};
        if(file.open(QIODevice::ReadOnly)){
            const auto lines {file.readAll().split('\n')};
            file.close();
            std::for_each(lines.begin(),lines.end(),[&](const QByteArray& line){
                const QString tempLine {line};
                if(!tempLine.startsWith("#") && !tempLine.isEmpty()){
                    const QJsonObject mountpointObject{
                        {"file_system",tempLine.section(' ',0,0,QString::SectionSkipEmpty).simplified()},
                        {"mount_point",tempLine.section(' ',1,1,QString::SectionSkipEmpty).simplified()},
                        {"mntpoint_type",tempLine.section(' ',2,2,QString::SectionSkipEmpty).simplified()},
                        {"options",tempLine.section(' ',3,3,QString::SectionSkipEmpty).simplified()},
                        {"dump",tempLine.section(' ',4,4,QString::SectionSkipEmpty).simplified()},
                        {"pass",tempLine.section(' ',5,5,QString::SectionSkipEmpty).simplified()}
                    };
                    mountpointObjects.push_back(mountpointObject);
                }
            });
        }
    }
    return mountpointObjects;
}

int main(int argc, char *argv[])
{
    QSet<QString> partitionPathSet {};
    auto getDeviceType{[](int type){
            const QMap<int,QString> typesMap {
                {0,"unknown"},
                {1,"scsi"},
                {2,"ide"},
                {3,"dac960"},
                {4,"cpqarray"},
                {5,"file"},
                {6,"ataraid"},
                {7,"i2o"},
                {8,"ubd"},
                {9,"dasd"},
                {10,"viodasd"},
                {11,"sx8"},
                {12,"dm"},
                {13,"xvd"},
                {14,"sd/mmc"},
                {15,"virtblk"},
                {16,"aoe"},
                {17,"md"},
                {18,"loopback"}
            };
            const auto it {typesMap.find(type)};
            if(it!=typesMap.cend()){
                return it.value();
            }
            return QString {"unknown"};
        }
    };

    auto getPartitionType{[](PedPartitionType type){
            const QMap<int,QString> typesMap {
                {PedPartitionType::PED_PARTITION_NORMAL,"normal"},
                {PedPartitionType::PED_PARTITION_LOGICAL,"logical"},
                {PedPartitionType::PED_PARTITION_EXTENDED,"extended"},
                {PedPartitionType::PED_PARTITION_FREESPACE,"freespace"},
                {PedPartitionType::PED_PARTITION_METADATA,"metadata"},
                {PedPartitionType::PED_PARTITION_PROTECTED,"protected"}
            };
            const auto it {typesMap.find(type)};
            if(it!=typesMap.cend()){
                return it.value();
            }
            return QString {"unknown"};
        }
    };

    //from fdisk.c/parted.c
    QJsonArray deviceObjects {};
    PedDevice *devicePtr {nullptr};
    ped_device_probe_all();

    //device
    while((devicePtr = ped_device_get_next(devicePtr))){
        QJsonObject diskObject {};

        //disk
        PedDisk* diskPtr {ped_disk_new(devicePtr)};
        if(diskPtr){
            const PedDiskType* diskTypePtr {diskPtr->type};
            const PedDiskTypeFeature diskFeature {diskTypePtr->features};
            const QString tableType {diskTypePtr->name};

            PedDiskFlag diskFlag {};
            diskFlag=ped_disk_flag_next(diskFlag);
            QJsonArray diskFlagNames {};
            while(diskFlag){
                if(ped_disk_get_flag(diskPtr,diskFlag)){
                    const QString diskFlagName {ped_disk_flag_get_name(diskFlag)};
                    diskFlagNames.push_back(diskFlagName);
                }
                diskFlag=ped_disk_flag_next(diskFlag);
            }

            diskObject.insert("table_type",tableType);
            diskObject.insert("disk_flags",diskFlagNames);

            //partitions
            QJsonArray partitionObjects {};
            PedPartition* partitionPtr {diskPtr->part_list};
            while(partitionPtr!=nullptr){
                if(!ped_partition_is_active(partitionPtr) || partitionPtr->type & PED_PARTITION_METADATA){
                    partitionPtr=ped_disk_next_partition(diskPtr,partitionPtr);
                    continue;
                }
                const QString partName {ped_partition_get_name(partitionPtr)};
                const QString partPath {ped_partition_get_path(partitionPtr)};
                const QString partType {getPartitionType(partitionPtr->type)};
                PedGeometry partGeometry {partitionPtr->geom};
                int partNumbber {partitionPtr->num};

                const PedFileSystemType* fileSystemTypePtr {partitionPtr->fs_type};
                QString partFileSystem {""};
                if(fileSystemTypePtr){
                    partFileSystem=fileSystemTypePtr->name;
                }

                PedPartitionFlag partitionFlag {};
                partitionFlag=ped_partition_flag_next((PedPartitionFlag)0);
                QJsonArray partitionFlagNames {};
                while(partitionFlag){
                    if(ped_partition_get_flag(partitionPtr,partitionFlag)){
                        const QString partitionFlagName {ped_partition_flag_get_name(partitionFlag)};
                        partitionFlagNames.push_back(partitionFlagName);
                    }
                    partitionFlag=ped_partition_flag_next(partitionFlag);
                }
                const bool partIsBootable {partitionFlagNames.contains("boot") ? true : false};

                //from libblkid
                QString partUuid {};
                {
                    blkid_probe blkidProbe {blkid_new_probe_from_filename(qPrintable(partPath))};
                    if(blkidProbe){
                        const char* uuid {};
                        blkid_do_fullprobe(blkidProbe);
                        blkid_probe_lookup_value(blkidProbe,"UUID",&uuid,NULL);
                        partUuid=QString::fromLatin1(uuid);
                        blkid_free_probe(blkidProbe);
                    }
                }

                const QJsonObject partitionObject {
                    {"partition_name",partName},
                    {"partition_path",partPath},
                    {"partition_file_system",partFileSystem},
                    {"partition_start",partGeometry.start},
                    {"partition_end",partGeometry.end},
                    {"partition_length",partGeometry.length},
                    {"partition_type",partType},
                    {"partition_number",partNumbber},
                    {"boot_partition",partIsBootable},
                    {"partition_uuid",partUuid},
                };

                partitionObjects.push_back(partitionObject);
                partitionPathSet.insert(partPath);
                partitionPtr=ped_disk_next_partition(diskPtr,partitionPtr);
            }
            ped_disk_destroy(diskPtr);
            diskPtr=nullptr;
            diskObject.insert("partitions",partitionObjects);
        }

        const PedCHSGeometry biosGeom {devicePtr->bios_geom};
        const PedCHSGeometry hwGeometry {devicePtr->hw_geom};

        const QString deviceModel {devicePtr->model};
        const QString deviceType {getDeviceType(devicePtr->type)};
        const QString devicePath {devicePtr->path};

        //from libblkid
        QString deviceUuid {};
        QString deviceLabel {};
        {
            blkid_probe blkidProbe {blkid_new_probe_from_filename(devicePtr->path)};
            if(blkidProbe){
                const char* uuid {};
                const char* label {};
                const char* serial {};
                const char* type {};
                blkid_do_fullprobe(blkidProbe);
                blkid_probe_lookup_value(blkidProbe,"UUID",&uuid,NULL);
                blkid_probe_lookup_value(blkidProbe,"LABEL",&label,NULL);
                blkid_probe_lookup_value(blkidProbe,"ID_SERIAL",&serial,NULL);
                blkid_probe_lookup_value(blkidProbe,"TYPE",&type,NULL);
                deviceUuid=QString::fromLatin1(uuid);
                deviceLabel=QString::fromLatin1(label);
                blkid_free_probe(blkidProbe);
            }
        }

        QString deviceSerialNum {};
        {
            static struct hd_driveid driveId {};
            int fd {open(qPrintable(devicePath),O_RDONLY|O_NONBLOCK)};
            if(fd > 0){
                if(ioctl(fd,HDIO_GET_IDENTITY,&driveId)){
                    deviceSerialNum=QString::fromLatin1((const char*)driveId.serial_no,sizeof(driveId.serial_no)).simplified();
                    qDebug("Serial: %s",qPrintable(deviceSerialNum));
                }
            }
        }

        const QJsonObject deviceObject {
            {"model",deviceModel},
            {"device_type",deviceType},
            {"path",devicePath},
            {"size",devicePtr->length * devicePtr->sector_size},
            {"logical_sector_size",devicePtr->sector_size},
            {"physical_sector_size",devicePtr->phys_sector_size},
            {"bios_cylinders",biosGeom.cylinders},
            {"bios_heads",biosGeom.heads},
            {"bios_sectors",biosGeom.sectors},
            {"bios_cylinder_size",biosGeom.heads * biosGeom.sectors},
            {"hw_cylinders",hwGeometry.cylinders},
            {"hw_heads",hwGeometry.heads},
            {"hw_sectors",hwGeometry.sectors},
            {"hw_cylinder_size",hwGeometry.heads * hwGeometry.sectors},
            {"device_uuid",deviceUuid},
            {"device_label",deviceLabel},
            {"disk",diskObject},
        };
        deviceObjects.push_back(deviceObject);
    }
    ped_device_free_all();

    QJsonArray partitionNextObjects {};
    QJsonArray physDeviceObjects {};
    std::for_each(qAsConst(deviceObjects).begin(),qAsConst(deviceObjects).end(),[&](const QJsonValue& jsonValue){
        const QJsonObject deviceObject {jsonValue.toObject()};
        const QJsonObject diskObject {deviceObject.value("disk").toObject()};
        const QJsonArray partitionArray=diskObject.value("partitions").toArray();

        QJsonArray partitionObjects {};
        std::for_each(qAsConst(partitionArray).begin(),qAsConst(partitionArray).end(),[&](const QJsonValue& jsonValue){
            const QJsonObject jsonObject {jsonValue.toObject()};
            partitionNextObjects.push_back(jsonObject);

            const QJsonObject partitionObject {
                {"partition_end",jsonObject.value("partition_end")},
                {"boot_partition",jsonObject.value("boot_partition")},
                {"partition_length",jsonObject.value("partition_length")},
                {"index",jsonObject.value("partition_number")},
                {"id_disk",jsonObject.value("partition_path")},
                {"name",jsonObject.value("partition_path")},
                {"start_sector",jsonObject.value("partition_start")},
                {"partition_type",jsonObject.value("partition_type")},
                {"device_id",jsonObject.value("partition_uuid")}
            };
            partitionObjects.push_back(partitionObject);
        });

        const QJsonObject physDiskObject {
            {"description",deviceObject.value("device_label").toString()},
            {"guid",deviceObject.value("device_uuid").toString()},
            {"bytes_per_sector",deviceObject.value("logical_sector_size").toDouble()},
            {"id_ph_disk",deviceObject.value("path").toString()},
            {"bytes_per_sector_physical",deviceObject.value("physical_sector_size").toDouble()},
            {"partitions",partitionObjects}
        };

        QJsonObject  physDeviceObject {
            {"bios_cylinder_size",deviceObject.value("bios_cylinder_size").toDouble()},
            {"bios_cylinders",deviceObject  .value("bios_cylinders").toDouble()},
            {"bios_heads",deviceObject.value("bios_heads").toDouble()},
            {"bios_sectors",deviceObject.value("bios_sectors").toDouble()},
            {"interface_type",deviceObject.value("device_type").toString()},
            {"tracks_per_cylinder",deviceObject .value("hw_cylinder_size").toDouble()},
            {"total_cylinders",deviceObject.value("hw_cylinders").toDouble()},
            {"total_heads",deviceObject.value("hw_heads").toDouble()},
            {"sectors_per_track",deviceObject.value("hw_sectors").toDouble()},
            {"disk_model",deviceObject.value("model").toString()},
            {"interface_connection",deviceObject.value("path").toString()},
            {"device_id",deviceObject.value("path").toString()},
            {"index",deviceObject.value("path").toString()},
            {"size",deviceObject.value("size").toDouble()},
            {"serial_number",""},
            {"disk_type",""},
            {"disk",physDiskObject}
        };

        physDeviceObjects.push_back(physDeviceObject);
    });

    const QJsonArray mountpointObjects {getMountpointObjects()};

    QJsonArray volumeObjects {};
    std::for_each(qAsConst(partitionNextObjects).begin(),qAsConst(partitionNextObjects).end(),[&](const QJsonValue  & jsonValue){
        const QJsonObject jsonObject {jsonValue.toObject()};
        if(jsonObject.contains("partition_file_system") && !jsonObject.value("partition_file_system").toString().isEmpty()){
            const QString partPath {jsonObject.value("partition_path").toString()};
            QJsonObject volumeObject {
                {"file_system",jsonObject.value("partition_file_system")},
                {"disk_index ",partPath},
                {"label",jsonObject.value("partition_name")},
                {"device_id",jsonObject.value("partition_uuid")}
            };
            const auto filesysMap {getFilesysObject(partPath)};
            if(!filesysMap.isEmpty()){
                auto begin {filesysMap.begin()};
                while(begin!=filesysMap.end()){
                    volumeObject.insert(begin.key(),QJsonValue::fromVariant(begin.value()));
                    ++begin;
                }
            }
            volumeObjects.push_back(volumeObject);
        }
    });

    const QJsonObject outObject {
        {"volumes",volumeObjects},
        {"mountpoints",mountpointObjects},
        {"physical_devices",physDeviceObjects}
    };
    std::cout<<QJsonDocument(outObject).toJson().toStdString();
    return 0;
}
