﻿using Microsoft.Win32.SafeHandles;
using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Management;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace MfsTool
{
    class Program
    {
        public enum EMoveMethod : uint
        {
            Begin = 0,
            Current = 1,
            End = 2
        }

        /* Needed for direct disk access */
        [DllImport("kernel32.dll", CharSet = CharSet.Auto, SetLastError = true)]
        internal static extern SafeFileHandle CreateFile(string lpFileName, uint
        dwDesiredAccess, uint dwShareMode,
        IntPtr lpSecurityAttributes, uint dwCreationDisposition, uint
        dwFlagsAndAttributes,
        SafeFileHandle hTemplateFile);
        const uint GENERIC_READ = 0x80000000;
        const uint GENERIC_WRITE = 0x050000000;
        public const int FILE_FLAG_NO_BUFFERING = 0x20000000;
        internal const int OPEN_EXISTING = 3;

        [DllImport("kernel32.dll", ExactSpelling = true, SetLastError = true, CharSet = CharSet.Auto)]
        private static extern bool DeviceIoControl(
            IntPtr hDevice,
            uint dwIoControlCode,
            IntPtr lpInBuffer,
            uint nInBufferSize,
            IntPtr lpOutBuffer,
            uint nOutBufferSize,
            out uint lpBytesReturned,
            IntPtr lpOverlapped
        );
        const int FSCTL_LOCK_VOLUME = 0x00090018;
        const int FSCTL_DISMOUNT_VOLUME = 0x00090020;
        const int FSCTL_UNLOCK_VOLUME = 0x00090022;

        [DllImport("kernel32", SetLastError = true)]
        static extern bool ReadFile(
            IntPtr hFile,
            byte[] lpBuffer,
            uint numBytesToRead,
            out uint numBytesRead,
            IntPtr lpOverlapped
        );

        [DllImport("kernel32.dll")]
        static extern bool WriteFile(
            IntPtr hFile,
            byte[] lpBuffer,
            uint nNumberOfBytesToWrite,
            out uint lpNumberOfBytesWritten,
            IntPtr lpOverlapped
        );

        [DllImport("kernel32.dll", EntryPoint = "SetFilePointer")]
        static extern uint SetFilePointer(
              [In] SafeFileHandle hFile,
              [In] int lDistanceToMove,
              [In, Out] ref int lpDistanceToMoveHigh,
              [In] EMoveMethod dwMoveMethod);

        [DllImport("kernel32.dll", ExactSpelling = true, SetLastError = true)]
        private static extern bool CloseHandle(SafeFileHandle handle);

        /* Helpers */
        static SafeFileHandle OpenDisk(String Id)
        {
            /* Open Disk */
            SafeFileHandle handle =
                CreateFile(Id,
                GENERIC_READ | GENERIC_WRITE, 0, IntPtr.Zero,
                OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, new SafeFileHandle(IntPtr.Zero, true));

            if (handle.IsInvalid)
            {
                Console.WriteLine("Failed to open drive");
                return null;
            }

            /* Lock Disk */
            uint lpBytesReturned = 0;
            if (!DeviceIoControl(handle.DangerousGetHandle(), FSCTL_LOCK_VOLUME, IntPtr.Zero, 0, IntPtr.Zero, 0, out lpBytesReturned, IntPtr.Zero))
                Console.WriteLine("Failed to lock!");
            if (!DeviceIoControl(handle.DangerousGetHandle(), FSCTL_DISMOUNT_VOLUME, IntPtr.Zero, 0, IntPtr.Zero, 0, out lpBytesReturned, IntPtr.Zero))
                Console.WriteLine("Failed to unmount!");

            /* done */
            return handle;
        }

        /* Write */
        static void WriteDisk(MfsDisk mDisk, SafeFileHandle Disk, UInt64 Sector, Byte[] Buffer)
        {
            uint BytesWritten = 0;
            UInt64 ValToMove = (Sector * mDisk.BytesPerSector);
            int DistHigh = (int)((ValToMove >> 32) & 0xFFFFFFFF);
            SetFilePointer(Disk, (int)(ValToMove & 0xFFFFFFFF), ref DistHigh, EMoveMethod.Begin);
            WriteFile(Disk.DangerousGetHandle(), Buffer, (uint)Buffer.Length, out BytesWritten, IntPtr.Zero);
        }

        /* Read */
        static Byte[] ReadDisk(MfsDisk mDisk, SafeFileHandle Disk, UInt64 Sector, UInt64 SectorCount)
        {
            /* Alloc buffer */
            Byte[] RetBuf = new Byte[SectorCount * mDisk.BytesPerSector];

            /* Seek */
            UInt64 ValToMove = (Sector * mDisk.BytesPerSector);
            int DistHigh = (int)((ValToMove >> 32) & 0xFFFFFFFF);
            SetFilePointer(Disk, (int)(ValToMove & 0xFFFFFFFF), ref DistHigh, EMoveMethod.Begin);

            /* Read */
            uint bRead = 0;
            ReadFile(Disk.DangerousGetHandle(), RetBuf, (uint)RetBuf.Length, out bRead, IntPtr.Zero);

            /* Done */
            return RetBuf;
        }

        /* Allocate a bucket */
        static UInt32 AllocateBucket(MfsDisk mDisk, SafeFileHandle hDisk, UInt32 FreeBucketIndex, UInt64 NumBuckets)
        {
            /* Calculate Bucket Map */
            UInt64 Buckets = mDisk.TotalSectors / mDisk.BucketSize;
            UInt64 BucketMapSize = Buckets * 4; /* One bucket descriptor is 4 bytes */
            UInt64 BucketMapSector = (mDisk.TotalSectors - ((BucketMapSize / mDisk.BytesPerSector) + 1));
            UInt32 BucketsPerSector = mDisk.BytesPerSector / 4;

            UInt64 Counter = NumBuckets;
            UInt32 BucketPtr = FreeBucketIndex;
            UInt32 BucketPrevPtr = 0;
            while (Counter > 0)
            {
                /* Which sector is the next bucket in? */
                UInt32 SectorOffset = BucketPtr / BucketsPerSector;
                UInt32 SectorIndex = BucketPtr % BucketsPerSector;

                /* Read sector */
                Byte[] Sector = ReadDisk(mDisk, hDisk, BucketMapSector + SectorOffset, 1);

                /* Done */
                BucketPrevPtr = BucketPtr;
                BucketPtr = BitConverter.ToUInt32(Sector, (int)(SectorIndex * 4));

                /* Next */
                Counter--;
            }

            /* Update BucketPrevPtr to 0xfFFFFFFF */
            UInt32 _SecOff = BucketPrevPtr / BucketsPerSector;
            UInt32 _SecInd = BucketPrevPtr % BucketsPerSector;

            /* Read sector */
            Byte[] _Sec = ReadDisk(mDisk, hDisk, BucketMapSector + _SecOff, 1);

            /* Modify Sector */
            _Sec[_SecInd * 4] = 0xFF;
            _Sec[_SecInd * 4 + 1] = 0xFF;
            _Sec[_SecInd * 4 + 2] = 0xFF;
            _Sec[_SecInd * 4 + 3] = 0xFF;

            /* Write it back */
            WriteDisk(mDisk, hDisk, BucketMapSector + _SecOff, _Sec);
            
            /* Done */
            return BucketPtr;
        }

        /* Locate next bucket */
        static UInt32 GetNextBucket(MfsDisk mDisk, SafeFileHandle hDisk, UInt32 Bucket)
        {
            /* Calculate Bucket Map */
            UInt64 Buckets = mDisk.TotalSectors / mDisk.BucketSize;
            UInt64 BucketMapSize = Buckets * 4; /* One bucket descriptor is 4 bytes */
            UInt64 BucketMapSector = (mDisk.TotalSectors - ((BucketMapSize / mDisk.BytesPerSector) + 1));

            /* Calculate Index */
            UInt32 BucketsPerSector = mDisk.BytesPerSector / 4;
            UInt32 SectorOffset = Bucket / BucketsPerSector;
            UInt32 SectorIndex = Bucket % BucketsPerSector;

            /* Read sector */
            Byte[] Sector = ReadDisk(mDisk, hDisk, BucketMapSector + SectorOffset, 1);

            /* Done */
            return BitConverter.ToUInt32(Sector, (int)(SectorIndex * 4));
        }

        /* Update bucket ptr */
        static void SetNextBucket(MfsDisk mDisk, SafeFileHandle hDisk, UInt32 Bucket, UInt32 NextBucket)
        {
            /* Calculate Bucket Map */
            UInt64 Buckets = mDisk.TotalSectors / mDisk.BucketSize;
            UInt64 BucketMapSize = Buckets * 4; /* One bucket descriptor is 4 bytes */
            UInt64 BucketMapSector = (mDisk.TotalSectors - ((BucketMapSize / mDisk.BytesPerSector) + 1));

            /* Calculate Index */
            UInt32 BucketsPerSector = mDisk.BytesPerSector / 4;
            UInt32 SectorOffset = Bucket / BucketsPerSector;
            UInt32 SectorIndex = Bucket % BucketsPerSector;

            /* Read sector */
            Byte[] Sector = ReadDisk(mDisk, hDisk, BucketMapSector + SectorOffset, 1);

            /* Edit */
            Sector[SectorIndex * 4] = (Byte)(NextBucket & 0xFF);
            Sector[SectorIndex * 4 + 1] = (Byte)((NextBucket >> 8) & 0xFF);
            Sector[SectorIndex * 4 + 2] = (Byte)((NextBucket >> 16) & 0xFF);
            Sector[SectorIndex * 4 + 3] = (Byte)((NextBucket >> 24) & 0xFF);

            /* Write */
            WriteDisk(mDisk, hDisk, BucketMapSector + SectorOffset, Sector);
        }

        /* Format a drive with mfs */
        static void Format(MfsDisk mDisk)
        {
            /* Open Disk */
            SafeFileHandle handle = OpenDisk(mDisk.DeviceId);

            if (handle == null)
            {
                Console.WriteLine("Failed to open drive");
                return;
            }

            /* Print */
            Console.WriteLine("Calculating metrics");

            /* Calc bytes */
            UInt64 DriveSizeBytes = mDisk.TotalSectors * mDisk.BytesPerSector;
            UInt64 GigaByte = (1024 * 1024 * 1024);
            UInt32 BucketSize = 0;
            UInt64 Buckets = 0;
            UInt64 BucketMapSize = 0;
            UInt64 BucketMapSector = 0;
            UInt64 ReservedBuckets = 0;
            UInt64 MirrorMasterBucketSector = 0;

            /* Determine bucket size 
             * if <1gb = 1 Kb (2 sectors) 
             * If <64gb = 4 Kb (8 sectors)
             * If >64gb = 8 Kb (16 sectors)
             * If >256gb = 16 Kb (32 sectors) */
            if (DriveSizeBytes >= (256 * GigaByte))
                BucketSize = 32;
            else if (DriveSizeBytes >= (64 * GigaByte))
                BucketSize = 16;
            else if (DriveSizeBytes <= GigaByte)
                BucketSize = 2;
            else
                BucketSize = 8;

            /* Print */
            mDisk.BucketSize = (UInt16)BucketSize;
            Console.WriteLine("Bucket Size: " + BucketSize.ToString());

            /* Get size of stage2-loader */
            FileStream nLoader = new FileStream("ssbl.stm", FileMode.Open);
            Byte[] Stage2Data = new Byte[nLoader.Length];
            nLoader.Read(Stage2Data, 0, (int)nLoader.Length);
            nLoader.Close();

            /* Calculate reserved sector count */
            Int64 ReservedSectors = 1 + ((Stage2Data.Length / mDisk.BytesPerSector) + 1);

            /* Print */
            Console.WriteLine("Reserved Sectors: " + ReservedSectors.ToString());

            /* Setup Bucket-list
             * SectorCount / BucketSize
             * Each bucket must point to the next, 
             * untill we reach the end of buckets
             * Position at end of drive */
            Buckets = mDisk.TotalSectors / BucketSize;
            BucketMapSize = Buckets * 4; /* One bucket descriptor is 4 bytes */
            BucketMapSector = (mDisk.TotalSectors - ((BucketMapSize / mDisk.BytesPerSector) + 1));
            MirrorMasterBucketSector = BucketMapSector - 1;

            /* Reserve an additional for the mirror bucket */
            ReservedBuckets = (((BucketMapSize / mDisk.BytesPerSector) + 1) / BucketSize) + 1;

            /* Print */
            Console.WriteLine("Bucket Count: " + Buckets.ToString());
            Console.WriteLine("Bucket Map Size: " + BucketMapSize.ToString());
            Console.WriteLine("Bucket Map Sector: " + BucketMapSector.ToString());
            Console.WriteLine("Reserved Buckets: " + ReservedBuckets.ToString());

            /* Step 1 */
            Console.WriteLine("Writing BucketTable");

            /* Seek to sector BucketMapSector */
            UInt64 ValToMove = BucketMapSector * mDisk.BytesPerSector;
            int DistHigh = (int)((ValToMove >> 32) & 0xFFFFFFFF);
            uint seekres = SetFilePointer(handle, (int)(ValToMove & 0xFFFFFFFF), ref DistHigh, EMoveMethod.Begin);
            Byte[] SectorBuffer = new Byte[mDisk.BytesPerSector];
            UInt64 Iterator = 0;
            UInt64 Max = Buckets;

            while (Iterator < Max)
            {
                /* Build sector buffer */
                for (UInt64 i = 0; i < (mDisk.BytesPerSector / 4); i++)
                {
                    /* Sanity */
                    if (Iterator >= ((Max - ReservedBuckets) - 1))
                    {
                        SectorBuffer[(i * 4)] = 0xFF;
                        SectorBuffer[(i * 4) + 1] = 0xFF;
                        SectorBuffer[(i * 4) + 2] = 0xFF;
                        SectorBuffer[(i * 4) + 3] = 0xFF;
                    }
                    else
                    {
                        SectorBuffer[(i * 4)] = (Byte)((Iterator + 1) & 0xFF);
                        SectorBuffer[(i * 4) + 1] = (Byte)(((Iterator + 1) >> 8) & 0xFF);
                        SectorBuffer[(i * 4) + 2] = (Byte)(((Iterator + 1) >> 16) & 0xFF);
                        SectorBuffer[(i * 4) + 3] = (Byte)(((Iterator + 1) >> 24) & 0xFF);
                    }

                    /* Go to next */
                    Iterator++;
                }

                /* Write sector */
                uint bWritten = 0;
                WriteFile(handle.DangerousGetHandle(), SectorBuffer, (uint)SectorBuffer.Length, out bWritten, IntPtr.Zero);
            }
            
            /* Calculate how many start-buckets we shall skip because of 
             * reserved sectors */
            Int64 SkipBuckets = (ReservedSectors / BucketSize) + 2;
            Int64 MasterBucketSector = ReservedSectors + 1;

            /* Step 2 */
            Console.WriteLine("Writing MasterBucket Original & Mirror");
            Console.WriteLine("Bucket Original Sector: " + MasterBucketSector.ToString());
            Console.WriteLine("Bucket Mirror Sector: " + MirrorMasterBucketSector.ToString());

            /* Setup the MasterBucket structure */
            UInt32 BucketStartFree = 0;

            while (BucketStartFree < ((MasterBucketSector / BucketSize) + 1))
                BucketStartFree++;

            Console.WriteLine("First Free Bucket: " + BucketStartFree.ToString());
            UInt32 RootIndex = BucketStartFree;
            BucketStartFree = AllocateBucket(mDisk, handle, BucketStartFree, 1);
            UInt32 BadBucketIndex = BucketStartFree;
            BucketStartFree = AllocateBucket(mDisk, handle, BucketStartFree, 1);
            Console.WriteLine("First Free Bucket after initial: " + BucketStartFree.ToString());

            /* Write it to the two sectors */
            Byte[] MasterBucket = new Byte[512];

            /* Zero It */
            for (int i = 0; i < 512; i++)
                MasterBucket[i] = 0;

            /* Setup Magic */
            MasterBucket[0] = 0x4D;
            MasterBucket[1] = 0x46;
            MasterBucket[2] = 0x53;
            MasterBucket[3] = 0x31;

            /* Setup flags */
            MasterBucket[4] = 0;
            MasterBucket[5] = 0;
            MasterBucket[6] = 0;
            MasterBucket[7] = 0;

            /* Setup free bucket pointer */
            MasterBucket[8] = (Byte)(BucketStartFree & 0xFF);
            MasterBucket[9] = (Byte)((BucketStartFree >> 8) & 0xFF);
            MasterBucket[10] = (Byte)((BucketStartFree >> 16) & 0xFF);
            MasterBucket[11] = (Byte)((BucketStartFree >> 24) & 0xFF);

            /* Setup Root Index */
            MasterBucket[12] = (Byte)(RootIndex & 0xFF);
            MasterBucket[13] = (Byte)((RootIndex >> 8) & 0xFF);
            MasterBucket[14] = (Byte)((RootIndex >> 16) & 0xFF);
            MasterBucket[15] = (Byte)((RootIndex >> 24) & 0xFF);

            /* Setup Bad Bucket Index */
            MasterBucket[16] = (Byte)(BadBucketIndex & 0xFF);
            MasterBucket[17] = (Byte)((BadBucketIndex >> 8) & 0xFF);
            MasterBucket[18] = (Byte)((BadBucketIndex >> 16) & 0xFF);
            MasterBucket[19] = (Byte)((BadBucketIndex >> 24) & 0xFF);

            /* Seek */
            WriteDisk(mDisk, handle, (UInt64)MirrorMasterBucketSector, MasterBucket);
            WriteDisk(mDisk, handle, (UInt64)MasterBucketSector, MasterBucket);

            /* Wipe new sectors */
            Byte[] Wipe = new Byte[BucketSize * mDisk.BytesPerSector];
            for (int i = 0; i < Wipe.Length; i++)
                Wipe[i] = 0;

            Console.WriteLine("Wiping Root");
            WriteDisk(mDisk, handle, (RootIndex * BucketSize), Wipe);

            Console.WriteLine("Wiping Bad Bucket List");
            WriteDisk(mDisk, handle, (BadBucketIndex * BucketSize), Wipe);

            /* Step 3 */
            Console.WriteLine("Writing Reserved Sectors");

            /* Allocate a new buffer */
            Byte[] ReservedBuffer = new Byte[((Stage2Data.Length / mDisk.BytesPerSector) + 1) * mDisk.BytesPerSector];
            Stage2Data.CopyTo(ReservedBuffer, 0);
            WriteDisk(mDisk, handle, 1, ReservedBuffer);

            /* Step 4 */
            Console.WriteLine("Writing Bootloader");

            /* Load the mbr, modify it */
            Byte[] Mbr = File.ReadAllBytes("Stage1.bin");

            /* Set magic */
            Mbr[3] = 0x4D;
            Mbr[4] = 0x46;
            Mbr[5] = 0x53;
            Mbr[6] = 0x31;

            /* Set version */
            Mbr[7] = 0x1;

            /* Set flags */
            Mbr[8] = 0;

            /* Disk metrics */
            Mbr[9] = 0x80;
            
            Mbr[10] = (Byte)(mDisk.BytesPerSector & 0xFF);
            Mbr[11] = (Byte)((mDisk.BytesPerSector >> 8) & 0xFF);

            /* Sectors Per Track */
            Mbr[12] = (Byte)(mDisk.SectorsPerTrack & 0xFF);
            Mbr[13] = (Byte)((mDisk.SectorsPerTrack >> 8) & 0xFF);

            /* Heads Per Cylinder */
            Mbr[14] = (Byte)(mDisk.TracksPerCylinder & 0xFF);
            Mbr[15] = (Byte)((mDisk.TracksPerCylinder >> 8) & 0xFF);

            Mbr[16] = (Byte)(mDisk.TotalSectors & 0xFF);
            Mbr[17] = (Byte)((mDisk.TotalSectors >> 8) & 0xFF);
            Mbr[18] = (Byte)((mDisk.TotalSectors >> 16) & 0xFF);
            Mbr[19] = (Byte)((mDisk.TotalSectors >> 24) & 0xFF);
            Mbr[20] = (Byte)((mDisk.TotalSectors >> 32) & 0xFF);
            Mbr[21] = (Byte)((mDisk.TotalSectors >> 40) & 0xFF);
            Mbr[22] = (Byte)((mDisk.TotalSectors >> 48) & 0xFF);
            Mbr[23] = (Byte)((mDisk.TotalSectors >> 56) & 0xFF);

            Mbr[24] = (Byte)(ReservedSectors & 0xFF);
            Mbr[25] = (Byte)((ReservedSectors >> 8) & 0xFF);

            Mbr[26] = (Byte)(BucketSize & 0xFF);
            Mbr[27] = (Byte)((BucketSize >> 8) & 0xFF);

            Mbr[28] = (Byte)(MasterBucketSector & 0xFF);
            Mbr[29] = (Byte)((MasterBucketSector >> 8) & 0xFF);
            Mbr[30] = (Byte)((MasterBucketSector >> 16) & 0xFF);
            Mbr[31] = (Byte)((MasterBucketSector >> 24) & 0xFF);
            Mbr[32] = (Byte)((MasterBucketSector >> 32) & 0xFF);
            Mbr[33] = (Byte)((MasterBucketSector >> 40) & 0xFF);
            Mbr[34] = (Byte)((MasterBucketSector >> 48) & 0xFF);
            Mbr[35] = (Byte)((MasterBucketSector >> 56) & 0xFF);

            Mbr[36] = (Byte)(MirrorMasterBucketSector & 0xFF);
            Mbr[37] = (Byte)((MirrorMasterBucketSector >> 8) & 0xFF);
            Mbr[38] = (Byte)((MirrorMasterBucketSector >> 16) & 0xFF);
            Mbr[39] = (Byte)((MirrorMasterBucketSector >> 24) & 0xFF);
            Mbr[40] = (Byte)((MirrorMasterBucketSector >> 32) & 0xFF);
            Mbr[41] = (Byte)((MirrorMasterBucketSector >> 40) & 0xFF);
            Mbr[42] = (Byte)((MirrorMasterBucketSector >> 48) & 0xFF);
            Mbr[43] = (Byte)((MirrorMasterBucketSector >> 56) & 0xFF);

            /* Set volume name */
            Mbr[44] = (Byte)'M';
            Mbr[45] = (Byte)'o';
            Mbr[46] = (Byte)'l';
            Mbr[47] = (Byte)'l';
            Mbr[48] = (Byte)'e';
            Mbr[49] = (Byte)'n';
            Mbr[50] = (Byte)'O';
            Mbr[51] = (Byte)'S';

            /* Write it */
            WriteDisk(mDisk, handle, 0, Mbr);

            /* Done */
            CloseHandle(handle);
        }

        /* Recursive List */
        static MfsEntry ListRecursive(MfsDisk mDisk, SafeFileHandle hDisk, UInt32 DirBucket, String pPath)
        {
            /* Get token */
            int iDex = pPath.IndexOf("/");
            String LookFor = pPath.Substring(0, iDex == -1 ? pPath.Length : iDex);
            
            /* EoP */
            if (String.IsNullOrEmpty(LookFor)
                || LookFor.Contains("."))
            {
                /* List files */
                UInt32 IteratorBucket = DirBucket;
                int End = 0;

                while (End == 0)
                {
                    UInt64 Sector = IteratorBucket * mDisk.BucketSize;

                    /* Gogo */
                    Byte[] fBuffer = ReadDisk(mDisk, hDisk, Sector, mDisk.BucketSize);

                    for (int i = 0; i < (mDisk.BucketSize * mDisk.BytesPerSector); i++)
                    {
                        /* EoC? */
                        if (fBuffer[i] == 0)
                        {
                            End = 1;
                            break;
                        }

                        /* Woah, parse */
                        int Len = 0;
                        while (fBuffer[i + 72 + Len] != 0)
                            Len++;
                        String Name = System.Text.Encoding.UTF8.GetString(fBuffer, i + 72, Len);

                        if (LookFor.Contains(".")
                            && Name.ToLower() == LookFor.ToLower())
                        {
                            /* Match */
                            MfsEntry nEntry = new MfsEntry();
                            nEntry.Name = Name;
                            nEntry.Size = BitConverter.ToUInt64(fBuffer, i + 56);
                            nEntry.AllocatedSize = BitConverter.ToUInt64(fBuffer, i + 64);
                            nEntry.Bucket = BitConverter.ToUInt32(fBuffer, i + 4);

                            nEntry.DirBucket = IteratorBucket;
                            nEntry.DirIndex = (uint)i;

                            /* Done */
                            return nEntry;
                        }
                        else
                        {
                            Console.WriteLine("Entry: " + Name + " (" + BitConverter.ToUInt64(fBuffer, i + 56).ToString() + " Bytes)");
                        }

                        /* Next */
                        i += 1024;
                    }

                    /* Get next bucket */
                    if (End == 0)
                        IteratorBucket = GetNextBucket(mDisk, hDisk, IteratorBucket);

                    /* End of list? */
                    if (IteratorBucket == 0xFFFFFFFF)
                        End = 1;
                }

                /* Done */
                return null;
            }

            return null;
        }

        /* Lists root directory */
        static void ListDirectory(MfsDisk mDisk, String Path)
        {
            /* Open Disk */
            SafeFileHandle handle = OpenDisk(mDisk.DeviceId);

            if (handle == null)
            {
                Console.WriteLine("Failed to open drive");
                return;
            }

            /* Read MBR */
            Byte[] Mbr = ReadDisk(mDisk, handle, 0, 1);

            /* Find MB Ptr */
            UInt64 MbSector = BitConverter.ToUInt64(Mbr, 28);
            mDisk.BucketSize = BitConverter.ToUInt16(Mbr, 26);

            /* Find Root Ptr in MB */
            Byte[] Mb = ReadDisk(mDisk, handle, MbSector, 1);
            UInt32 RootBucket = BitConverter.ToUInt32(Mb, 12);

            /* Recurse-Parse Root */
            Console.WriteLine("Files in " + Path + ":");
            ListRecursive(mDisk, handle, RootBucket, Path);
            Console.WriteLine("");

            /* Done */
            CloseHandle(handle);
        }

        /* Create entry in Mb */
        static void CreateFileEntry(MfsDisk mDisk, SafeFileHandle hDisk, UInt16 Flags, UInt32 Bucket, Byte[] Data, String Name, UInt32 DirBucket)
        {
            /* List files */
            UInt32 IteratorBucket = DirBucket;
            int End = 0;

            while (End == 0)
            {
                UInt64 Sector = IteratorBucket * mDisk.BucketSize;

                /* Gogo */
                Byte[] fBuffer = ReadDisk(mDisk, hDisk, Sector, mDisk.BucketSize);

                for (int i = 0; i < (mDisk.BucketSize * mDisk.BytesPerSector); i++)
                {
                    /* EoC? */
                    if (fBuffer[i] == 0)
                    {
                        UInt64 NumBuckets = (UInt64)(Data.LongLength / mDisk.BytesPerSector) / mDisk.BucketSize;
                        if (((Data.LongLength / mDisk.BytesPerSector) % mDisk.BucketSize) > 0)
                            NumBuckets++;
                        UInt64 AllocatedSize = NumBuckets * mDisk.BucketSize * mDisk.BytesPerSector;

                        /* Setup Status */
                        fBuffer[i] = 0x1;
                        fBuffer[i + 1] = 0x0;

                        /* Setup flags */
                        fBuffer[i + 2] = (Byte)(Flags & 0xFF);
                        fBuffer[i + 3] = (Byte)((Flags >> 8) & 0xFF);

                        /* Setup start bucket */
                        fBuffer[i + 4] = (Byte)(Bucket & 0xFF);
                        fBuffer[i + 5] = (Byte)((Bucket >> 8) & 0xFF);
                        fBuffer[i + 6] = (Byte)((Bucket >> 16) & 0xFF);
                        fBuffer[i + 7] = (Byte)((Bucket >> 24) & 0xFF);

                        /* Ignore time and date for now */
                        fBuffer[i + 56] = (Byte)(Data.LongLength & 0xFF);
                        fBuffer[i + 57] = (Byte)((Data.LongLength >> 8) & 0xFF);
                        fBuffer[i + 58] = (Byte)((Data.LongLength >> 16) & 0xFF);
                        fBuffer[i + 59] = (Byte)((Data.LongLength >> 24) & 0xFF);
                        fBuffer[i + 60] = (Byte)((Data.LongLength >> 32) & 0xFF);
                        fBuffer[i + 61] = (Byte)((Data.LongLength >> 40) & 0xFF);
                        fBuffer[i + 62] = (Byte)((Data.LongLength >> 48) & 0xFF);
                        fBuffer[i + 63] = (Byte)((Data.LongLength >> 56) & 0xFF);

                        fBuffer[i + 64] = (Byte)(AllocatedSize & 0xFF);
                        fBuffer[i + 65] = (Byte)((AllocatedSize >> 8) & 0xFF);
                        fBuffer[i + 66] = (Byte)((AllocatedSize >> 16) & 0xFF);
                        fBuffer[i + 67] = (Byte)((AllocatedSize >> 24) & 0xFF);
                        fBuffer[i + 68] = (Byte)((AllocatedSize >> 32) & 0xFF);
                        fBuffer[i + 69] = (Byte)((AllocatedSize >> 40) & 0xFF);
                        fBuffer[i + 70] = (Byte)((AllocatedSize >> 48) & 0xFF);
                        fBuffer[i + 71] = (Byte)((AllocatedSize >> 56) & 0xFF);

                        /* Setup Name */
                        Byte[] NameData = System.Text.Encoding.UTF8.GetBytes(Name);
                        for (int j = 0; j < NameData.Length; j++)
                            fBuffer[i + 72 + j] = NameData[j];

                        /* Don't use rest for now */
                        WriteDisk(mDisk, hDisk, Sector, fBuffer);

                        /* Done */
                        break;
                    }

                    /* Next */
                    i += 1024;
                }

                /* Get next bucket */
                if (End == 0)
                    IteratorBucket = GetNextBucket(mDisk, hDisk, IteratorBucket);

                /* End of list? */
                if (IteratorBucket == 0xFFFFFFFF)
                    End = 1;
            }
        }

        /* Fill bucketchain with data */
        static void FillBucketChain(MfsDisk mDisk, SafeFileHandle hDisk, UInt32 Bucket, Byte[] Data)
        {
            /* Alloc Buffer */
            Byte[] Buffer = new Byte[mDisk.BucketSize * mDisk.BytesPerSector];

            /* Index */
            Int64 Index = 0;
            UInt32 BucketPtr = Bucket;

            /* Iterate */
            while (Index < Data.LongLength)
            {
                /* Copy */
                for (int i = 0; i < Buffer.Length; i++)
                {
                    if (Index + i >= Data.LongLength)
                        Buffer[i] = 0;
                    else
                        Buffer[i] = Data[Index + i];
                }
                    
                /* Inc */
                Index += Buffer.Length;

                /* Calculate target sector */
                UInt64 Sector = mDisk.BucketSize * BucketPtr;
                WriteDisk(mDisk, hDisk, Sector, Buffer);

                /* Get next bucket */
                BucketPtr = GetNextBucket(mDisk, hDisk, BucketPtr);

                /* Sanity */
                if (BucketPtr == 0xFFFFFFFF)
                    break;
            }
        }

        /* Write file */
        static void WriteToMfs(MfsDisk mDisk, String pFile)
        {
            /* Open Disk */
            SafeFileHandle handle = OpenDisk(mDisk.DeviceId);

            if (handle == null)
            {
                Console.WriteLine("Failed to open drive");
                return;
            }

            /* Read MBR */
            Byte[] Mbr = ReadDisk(mDisk, handle, 0, 1);

            /* Find MB Ptr */
            UInt64 MbSector = BitConverter.ToUInt64(Mbr, 28);
            UInt64 MbMirrorSector = BitConverter.ToUInt64(Mbr, 36);
            mDisk.BucketSize = BitConverter.ToUInt16(Mbr, 26);

            /* Find Root Ptr in MB */
            Byte[] Mb = ReadDisk(mDisk, handle, MbSector, 1);
            UInt32 FreeBucket = BitConverter.ToUInt32(Mb, 8);
            UInt32 RootBucket = BitConverter.ToUInt32(Mb, 12);

            /* Load File */
            Console.WriteLine("Reading source data");
            Byte[] FileData = File.ReadAllBytes(pFile);

            /* Can we even write a file there */
            MfsEntry nEntry = ListRecursive(mDisk, handle, RootBucket, Path.GetFileName(pFile));
            if (nEntry != null)
            {
                Console.WriteLine("File exists in table, updating");

                /* Sanity */
                if ((UInt64)FileData.LongLength > nEntry.AllocatedSize)
                {
                    /* Allocate more */
                    UInt64 NumSectors = ((UInt64)FileData.LongLength - nEntry.AllocatedSize) / mDisk.BytesPerSector;
                    if ((((UInt64)FileData.LongLength - nEntry.AllocatedSize) % mDisk.BytesPerSector) > 0)
                        NumSectors++;

                    UInt64 NumBuckets = NumSectors / mDisk.BucketSize;
                    if ((NumSectors % mDisk.BucketSize) > 0)
                        NumBuckets++;

                    /* Allocate buckets */
                    UInt32 StartBucket = FreeBucket;
                    FreeBucket = AllocateBucket(mDisk, handle, FreeBucket, NumBuckets);

                    /* Get last bucket in chain */
                    UInt32 BucketPtr = nEntry.Bucket;
                    UInt32 BucketPrevPtr = 0;
                    while (BucketPtr != 0xFFFFFFFF)
                    {
                        BucketPrevPtr = BucketPtr;
                        BucketPtr = GetNextBucket(mDisk, handle, BucketPtr);
                    }

                    /* Update pointer */
                    SetNextBucket(mDisk, handle, BucketPrevPtr, FreeBucket);

                    /* Update MB */
                    Mb[8] = (Byte)(FreeBucket & 0xFF);
                    Mb[9] = (Byte)((FreeBucket >> 8) & 0xFF);
                    Mb[10] = (Byte)((FreeBucket >> 16) & 0xFF);
                    Mb[11] = (Byte)((FreeBucket >> 24) & 0xFF);

                    /* Write Mb */
                    WriteDisk(mDisk, handle, MbSector, Mb);
                    WriteDisk(mDisk, handle, MbMirrorSector, Mb);

                    /* Adjust */
                    nEntry.AllocatedSize += (NumBuckets * mDisk.BucketSize * mDisk.BytesPerSector);
                }

                /* If file size is drastically less, free some buckets */

                /* Write Data */
                Console.WriteLine("Updating Data");
                FillBucketChain(mDisk, handle, nEntry.Bucket, FileData);

                /* Update entry with new sizes, new dates, new times */
                Byte[] fBuffer = ReadDisk(mDisk, handle, (nEntry.DirBucket * mDisk.BucketSize), mDisk.BucketSize);
                
                /* Modify */
                fBuffer[nEntry.DirIndex + 4] = (Byte)(nEntry.Bucket & 0xFF);
                fBuffer[nEntry.DirIndex + 5] = (Byte)((nEntry.Bucket >> 8) & 0xFF);
                fBuffer[nEntry.DirIndex + 6] = (Byte)((nEntry.Bucket >> 16) & 0xFF);
                fBuffer[nEntry.DirIndex + 7] = (Byte)((nEntry.Bucket >> 24) & 0xFF);

                fBuffer[nEntry.DirIndex + 56] = (Byte)(FileData.LongLength & 0xFF);
                fBuffer[nEntry.DirIndex + 57] = (Byte)((FileData.LongLength >> 8) & 0xFF);
                fBuffer[nEntry.DirIndex + 58] = (Byte)((FileData.LongLength >> 16) & 0xFF);
                fBuffer[nEntry.DirIndex + 59] = (Byte)((FileData.LongLength >> 24) & 0xFF);
                fBuffer[nEntry.DirIndex + 60] = (Byte)((FileData.LongLength >> 32) & 0xFF);
                fBuffer[nEntry.DirIndex + 61] = (Byte)((FileData.LongLength >> 40) & 0xFF);
                fBuffer[nEntry.DirIndex + 62] = (Byte)((FileData.LongLength >> 48) & 0xFF);
                fBuffer[nEntry.DirIndex + 63] = (Byte)((FileData.LongLength >> 56) & 0xFF);

                fBuffer[nEntry.DirIndex + 64] = (Byte)(nEntry.AllocatedSize & 0xFF);
                fBuffer[nEntry.DirIndex + 65] = (Byte)((nEntry.AllocatedSize >> 8) & 0xFF);
                fBuffer[nEntry.DirIndex + 66] = (Byte)((nEntry.AllocatedSize >> 16) & 0xFF);
                fBuffer[nEntry.DirIndex + 67] = (Byte)((nEntry.AllocatedSize >> 24) & 0xFF);
                fBuffer[nEntry.DirIndex + 68] = (Byte)((nEntry.AllocatedSize >> 32) & 0xFF);
                fBuffer[nEntry.DirIndex + 69] = (Byte)((nEntry.AllocatedSize >> 40) & 0xFF);
                fBuffer[nEntry.DirIndex + 70] = (Byte)((nEntry.AllocatedSize >> 48) & 0xFF);
                fBuffer[nEntry.DirIndex + 71] = (Byte)((nEntry.AllocatedSize >> 56) & 0xFF);

                /* Write back */
                WriteDisk(mDisk, handle, (nEntry.DirBucket * mDisk.BucketSize), fBuffer);
            }
            else
            {
                Console.WriteLine("/" + Path.GetFileName(pFile) + " is new, creating");

                /* Get first free bucket */
                UInt64 NumBuckets = (UInt64)(FileData.LongLength / mDisk.BytesPerSector) / mDisk.BucketSize;
                if (((FileData.LongLength / mDisk.BytesPerSector) % mDisk.BucketSize) > 0)
                    NumBuckets++;

                /* Allocate a chain */
                Console.WriteLine("Allocing " + NumBuckets.ToString() + " buckets, old free ptr " + FreeBucket.ToString());
                UInt32 StartBucket = FreeBucket;
                FreeBucket = AllocateBucket(mDisk, handle, FreeBucket, NumBuckets);
                Console.WriteLine("Done, new free pointer " + FreeBucket.ToString());

                /* Create entry */
                Console.WriteLine("Creating entry in root-bucket");
                CreateFileEntry(mDisk, handle, 0x1, StartBucket, FileData, Path.GetFileName(pFile), RootBucket);
                Console.WriteLine("Done");

                /* Update MB */
                Mb[8] = (Byte)(FreeBucket & 0xFF);
                Mb[9] = (Byte)((FreeBucket >> 8) & 0xFF);
                Mb[10] = (Byte)((FreeBucket >> 16) & 0xFF);
                Mb[11] = (Byte)((FreeBucket >> 24) & 0xFF);

                /* Write Mb */
                WriteDisk(mDisk, handle, MbSector, Mb);
                WriteDisk(mDisk, handle, MbMirrorSector, Mb);

                /* Write Data */
                Console.WriteLine("Writing Data");
                FillBucketChain(mDisk, handle, StartBucket, FileData);
            }

            /* Done */
            Console.WriteLine("File Creation Done");
            CloseHandle(handle);
        }

        /* Entry */
        static void Main(string[] args)
        {
            /* Print Header */
            Console.WriteLine("MFS Utility Software");
            Console.WriteLine("Software Capabilities include formatting, read, write to/from MFS.\n");
            
            /* Get a list of physical drives */
            Hashtable Drives = new Hashtable();
            Console.WriteLine("Available Drives:");
            WqlObjectQuery q = new WqlObjectQuery("SELECT * FROM Win32_DiskDrive");
            ManagementObjectSearcher res = new ManagementObjectSearcher(q);
            int Itr = 0;
            foreach (ManagementObject o in res.Get()) {
                Console.WriteLine(Itr.ToString() + ". " + o["Caption"] + " (DeviceID = " + o["DeviceID"] + ")");
                
                /* Create Object */
                MfsDisk nDisk = new MfsDisk();
                nDisk.BytesPerSector = (UInt32)o["BytesPerSector"];
                nDisk.DeviceId = (String)o["DeviceID"];
                nDisk.TotalSectors = (UInt64)o["TotalSectors"];
                nDisk.SectorsPerTrack = (UInt32)o["SectorsPerTrack"];
                nDisk.TracksPerCylinder = (UInt32)o["TracksPerCylinder"];

                Drives.Add(Itr, nDisk);
                Itr++;
            }

            Console.WriteLine("\nAvailable Commands:");
            Console.WriteLine("format <drive>");
            Console.WriteLine("write <file> <drive>");
            Console.WriteLine("ls <path> <drive>");
            Console.WriteLine("quit");
            Console.WriteLine("");

            /* Cmd */
            while (true)
            {
                /* Spit */
                Console.Write(" $ ");

                /* Read */
                String Input = Console.ReadLine();
                String[] Tokens = Input.Split(new Char[] { ' ' });

                /* Lets see */
                switch (Tokens[0].ToLower())
                {
                    case "format":
                        {
                            /* Parse */
                            int Option = int.Parse(Tokens[1]);

                            /* Do the format */
                            Format((MfsDisk)Drives[Option]);

                        } break;
                    case "write":
                        {
                            /* Path */
                            String Path = Tokens[1];

                            /* Parse */
                            int Option = int.Parse(Tokens[2]);

                            /* Gogo */
                            WriteToMfs((MfsDisk)Drives[Option], Path);

                        } break;
                    case "ls":
                        {
                            /* Path */
                            String Path = Tokens[1];

                            /* Parse */
                            int Option = int.Parse(Tokens[2]);

                            /* Gogo */
                            ListDirectory((MfsDisk)Drives[Option], Path);

                        } break;
                    case "quit":
                            return;

                    default:
                        break;
                }

                /* Clean */
                GC.Collect();
            }
        }
    }

    /* Represents a disk */
    public class MfsDisk
    {
        /* Bytes Per Sector */
        public UInt32 BytesPerSector;
        public UInt32 SectorsPerTrack;
        public UInt32 TracksPerCylinder;

        /* Capacity */
        public UInt64 TotalSectors;

        /* Physical Drive No */
        public String DeviceId;

        /* Used by reading */
        public UInt16 BucketSize;
    }

    /* Represents a Mfs Entry */
    public class MfsEntry
    {
        /* Name */
        public String Name;
        public UInt64 Size;
        public UInt64 AllocatedSize;
        public UInt32 Bucket;


        public UInt32 DirBucket;
        public UInt32 DirIndex;
    }
}